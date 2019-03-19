/*
 * MLAB100 device application software.
 *
 * Copyright (C) 2019, Microbiota Labs Inc. and the MLAB100 contributors.
 *
 */
//=============================================================================

/* If we share sources between frameworks then we can identify esp-idf builds by
   the presence of the ESP_PLATFORM manifest. */
#if !defined(ESP_PLATFORM)
# error "Framework not supported - source is currently ESP32 only"
#endif // !ESP_PLATFORM

//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_types.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_spi_flash.h"

#include "nvs_flash.h"
#include "soc/efuse_reg.h"
#include "esp_ota_ops.h"

#include "esp_wifi.h"

#if !defined(ETH_ALEN)
# define ETH_ALEN (6)
#endif // !ETH_ALEN

#include "driver/gpio.h"

#include "mlab100.h"
#include "adc122s021.h"
#include "ds18b20.h"
#include "heater.h"
#include "onewire.h"

#include "mlab_blufi.h"
#include "mlab_webserver.h"

//-----------------------------------------------------------------------------

static const char * const p_tag = "mblMain"; // esp-idf logging prefix (tag) for this module

const char * const p_owner = "Microbiota Labs"; // Project owner
const char * const p_model = MLAB_MODEL; // H/W model identifier
const char * const p_version = STRINGIFY(MLAB_VERSION); // F/W version identifier
const char * const p_device = "MLAB" MLAB_MODEL; // Human-readable device name

static onewire_search_t onewire_ds;
#define MAX_SENSORS 3
static onewire_addr_t sensors[MAX_SENSORS];

static  TickType_t xLastWakeTime;

static heater_t heater;

//-----------------------------------------------------------------------------

/* The binary data passed between the Client and Server application layers is a
   simple set of tuples. There are no gaps and it is expected that the packet
   length encompasses a valid tuple though processing code should validate that
   the supplied "len" does NOT exceed the total binary blob boundary. */
typedef struct ml_app_tuple {
    uint8_t len; // length of this field
    uint8_t code; // operation encoding
    uint8_t data[0]; // zero or more bytes of "(len - 1)" data bytes
} __attribute__((packed)) ml_app_tuple_t;

// "code" values:
#define OPCODE_EXTENSION (0x00) // use next byte for "code"
/* TODO:DEFINE: initial set of requests/actions/data we need to pass between the Client and Server implementations. */
// all other codes are currently undefined and IGNORED

//-----------------------------------------------------------------------------
/* NOTE: The FreeRTOS implementation used by esp-idf does NOT support
   MessageBuffer (StreamBuffer) which could be a good wrapper onto
   single-write/single-reader copy based messaging. */

/* We pass data by reference between the receiver(s) and the main application
   loop. This approach allows multiple writes, for the single control loop
   reader and avoids an overly large queue element. */
typedef struct event_appdata {
    uint32_t len;
    uint8_t *p_data; // arbitrary data of "len" bytes
} event_appdata_t;

//-----------------------------------------------------------------------------
/* We use a FreeRTOS queue to transfer received data from the BLE-GATT world to
   the main application control loop. */

#define QUEUE_DEPTH_APPDATA (4) // maximum number of "in-flight" queued items in mailbox

static xQueueHandle queue_appdata = NULL;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
/* Since this message queue is a critical infrastructure item we statically
   allocate its resources to avoid low dynamic run-time memory affecting this
   core control mechanism. */
static event_appdata_t queue_storage_appdata[QUEUE_DEPTH_APPDATA];
static StaticQueue_t queue_appdata;
#endif // configSUPPORT_STATIC_ALLOCATION

//-----------------------------------------------------------------------------
// Exported to common mlab BluFi support

void mlab_app_data(const uint8_t *p_buffer,uint32_t blen)
{
    /* WARNING: This function should NOT block.
       If we need to pass the binary to another thread we should make a COPY. */
    if (queue_appdata) {
        ESP_LOGD(p_tag,"AppData blen %u",blen);

        /* CONSIDER: Ideally we would NOT be using dynamic memory allocation
           here (since it can create fragmentation arguments or lead to
           transient undesirable behaviour. We should re-implement using a
           pre-allocated message/stream buffer equivalent, */

        uint8_t *p_copy = (uint8_t *)malloc(blen);
        if (p_copy) {
            event_appdata_t appdata;

            (void)memcpy(p_copy,p_buffer,blen);

            appdata.p_data = p_copy;
            appdata.len = blen;

            if (pdTRUE != xQueueSendToBack(queue_appdata,&appdata,0)) {
                ESP_LOGE(p_tag,"Dropped AppData blen %u since queue full",blen);
            }
        } else {
            ESP_LOGW(p_tag,"Dropped AppData blen %u due to OOM",blen);
        }
    } else {
        ESP_LOGW(p_tag,"Dropped AppData blen %u since no queue",blen);
    }

    return;
}

//-----------------------------------------------------------------------------
// Used in the heater controller

// Get the average temperature in C from sensors
static float get_temperature(void) {
    float res = 0.;
    int i;

    for (i = 0; i < MAX_SENSORS; i++)
        res += ds18b20_get_temp(sensors[i]);

    return (res / MAX_SENSORS);
}

//-----------------------------------------------------------------------------
/* Since the app_main() functionality below provides multiple control loops
   depending on its state, we provide this single implementation for checking
   the BluFi interaction (and any other processing we want common to the
   different control loops). */

static void app_main_control(void)
{
    event_appdata_t appdata;
    const TickType_t xFrequency = (1000 / portTICK_PERIOD_MS);

   /* TODO: If we want the temperature control to be synchronised at some
       frequency we should do it off of a timer worker or similar rather
       than a vTaskDelay() in this loop; since we may want higher frequency
       polling of other subsystems (e.g. BluFi events) by this control loop. */

    // Wait for the next cycle.
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    heater.temperature = get_temperature();
    printf("[%d] mode: %s temperature: %0.1f\n", xTaskGetTickCount(), STATE2STR(heater.state), heater.temperature);

    switch (heater.state) {
        case IDLE:
            // the device heater is idle: do nothing
            break;
        case COOLING:
            if (heater.temperature < (heater.setpoint - heater.histeresis)) {
                // switch to heating mode
                HEATER_ON();
                heater.state = HEATING;
            }
            break;
        case HEATING:
            if (heater.temperature > (heater.setpoint - heater.adjustment)) {
                if (heater.temperature > (heater.setpoint + heater.histeresis)) {
                    // switch to cooling mode
                    HEATER_OFF();
                    heater.state = COOLING;                   
                }
            }
            break; 
    }


    // Do not block waiting for data:
    if (xQueueReceive(queue_appdata,&appdata,0)) {
        if (appdata.p_data && appdata.len) {
            ESP_LOGD(p_tag,"AppData foreground len %u",appdata.len);

            /* This is just a simple initial implementation. We may want to be
               triggering other application events (e.g. sampling data, starting
               a process, etc.) depending on the actions/events/data
               received. */
#if 1 // DIAGnostic
            uint8_t *p_tuple = appdata.p_data;
            uint32_t remaining = appdata.len;

            while (remaining) {
                uint8_t tlen = *p_tuple++;

                remaining--;

                if (remaining <  tlen) {
                    ESP_LOGW(p_tag,"Tuple length %u exceeds remaining %u : ignoring",tlen,remaining);
                    remaining = 0; // terminate scan
                } else {
                    uint32_t opcode = 0;
                    uint32_t level = 0;
                    do {
                        if (remaining < tlen) {
                            ESP_LOGW(p_tag,"Tuple length %u exceeds remaining %u (code scan): ignoring",tlen,remaining);
                            remaining = 0; // terminate scan
                            break;
                        }

                        uint8_t code = *p_tuple++;

                        tlen--; // length of remaining data
                        remaining--; // remaining in binary buffer

                        if (OPCODE_EXTENSION == code) {
                            level++;
                            if ((1 << 24) == level) {
                                ESP_LOGW(p_tag,"Tuple code scan level exceeds limit: ignoring");
                                remaining = 0;
                                break;
                            }
                        } else {
                            opcode = ((level << 8) | code);
                        }
                    } while (0 == opcode);

                    if (opcode) {
                        ESP_LOGI(p_tag,"AppData opcode 0x%" PRIX32 " data %u-byte%s",opcode,tlen,((1 == tlen) ? "" : "s"));
                        if (tlen) {
                            esp_log_buffer_hex(" OpcodeData",p_tuple,tlen);
                        }

                        /* IMPLEMENT: Whatever actions are needed for the opcode value */

                        /* e.g. trigger a set (i.e. multiple items if needed) of
                          individually "code"-identified data via:

                             build_tuples_into_somebuffer;
                             mlab_app_data_send(somebuffer,sizeof(somebuffer)
                        */
                    }

                    if (remaining) {
                        remaining -= tlen;
                    }
                }
            }
#endif // boolean
        }

        free(appdata.p_data);
    }

    return;
}

//-----------------------------------------------------------------------------

void app_main(void)
{
    ESP_LOGI(p_tag,"Application starting " STRINGIFY(MLAB_VERSION) " (BUILDTIMESTAMP %" PRIX64 ")",(uint64_t)BUILDTIMESTAMP);

    printf("--------------------------------------\n");
    printf(" %s: Starting %s - version: %s\n", p_owner, p_device, p_version);
    printf("--------------------------------------\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash.\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    fflush(stdout);

#if 1 // Some information that may be useful when checking OTA operation:
    {
        const esp_partition_t *p_configured = esp_ota_get_boot_partition();
        const esp_partition_t *p_running = esp_ota_get_running_partition();

        if (p_configured != p_running) {
            ESP_LOGW(p_tag,"Configured OTA boot partition at offset 0x%08X but executing from offset 0x%08X",p_configured->address,p_running->address);
        }
        ESP_LOGI(p_tag,"Executing from partition type %d subtype %d (offset 0x%08X)",p_running->type,p_running->subtype,p_running->address);
    }
#endif // boolean

#if 1 // If we need the MAC base address BEFORE we initialise the networking interfaces
    {
        uint8_t defmac[ETH_ALEN];
        esp_err_t rcode = esp_efuse_mac_get_default(defmac);
        if (ESP_OK == rcode) {
            ESP_LOGI(p_tag,"Default base MAC " PRIXMAC "",PRINTMAC(defmac));
            /* We could call esp_base_mac_addr_set(defmac) to stop warnings
               about using the default (factory) BLK0 value, but the code really
               only expects that function to be called if BLK3 or external MAC
               storage is being used; and by default the code will use the BLK0
               value as needed. */
            /* Derived from base address:
               - +0 WiFi STA
               - +1 WiFi AP
               - +2 Bluetooth
               - +3 Ethernet

               See the system_api.c:esp_read_mac(mac,type) function for more
               information.
             */
        } else {
            ESP_LOGE(p_tag,"Failed to read default MAC");
        }
    }
#endif // boolean

    // Initialize NVS — it is used to store PHY calibration data:
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(p_tag,"esp-idf " IDF_VER);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    queue_appdata = xQueueCreateStatic(QUEUE_DEPTH_APPDATA,sizeof(event_appdata_t),(uint8_t *)queue_storage_appdata,&queue_appdata);
#else // dynamic
    queue_appdata = xQueueCreate(QUEUE_DEPTH_APPDATA,sizeof(event_appdata_t));
#endif // dynamic
    if (NULL == queue_appdata) {
        ESP_LOGE(p_tag,"Failed to create main appdata queue");
    }

    app_common_platform_init();
#if defined(CONFIG_MLAB_HTTPD) && CONFIG_MLAB_HTTPD
    /* TODO:DECIDE: This is a quick hack solution to track the webserver
       handle. We probably want to pass a private context structure one field of
       which is the webserver handle. We should decide how we manage that
       context and pass the structure reference here instead and update the
       common/shared mlab/src/webserver.c and wifi.c accordingly. */
    static httpd_handle_t webserver = NULL;
    void *p_ctx = &webserver;
#else // !CONFIG_MLAB_HTTPD
    void *p_ctx = NULL;
#endif // !CONFIG_MLAB_HTTPD
    if (ESP_OK == wifi_initialise(p_ctx)) {
#if defined(CONFIG_MLAB_BLUFI) && CONFIG_MLAB_BLUFI
        if (ESP_OK != mlab_blufi_start()) {
            ESP_LOGE(p_tag,"Failed to start BluFi");
        }
#endif // CONFIG_MLAB_BLUFI
    } else {
        ESP_LOGE(p_tag,"Failed to initialise WiFi");
    }

    spi_device_handle_t opamp_adc = app_init_spi();
    ESP_LOGI(p_tag,"opamp_adc %p",opamp_adc);
 
    // initialize GPIO output pins -- onewire is setup by its own library
    #define GPIO_OUTPUT_PIN_SEL  ((1ULL<<FAN_CTRL) | (1ULL<<CONTROL_3V3) | (1ULL<<GREEN_LED) | (1ULL<<YELLOW_LED) | (1ULL<<RED_LED) | (1ULL<<UV1_LED) | (1ULL<<UV2_LED) )

    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    // turn on 3.3V
    gpio_set_direction(CONTROL_3V3, GPIO_MODE_OUTPUT);
    gpio_set_level(CONTROL_3V3, 1);

    gpio_pad_select_gpio(GREEN_LED);
    gpio_pad_select_gpio(YELLOW_LED);
    // gpio_pad_select_gpio(RED_LED);
    gpio_pad_select_gpio(CONTROL_3V3);

    // initialize the heater
    heater_init();

    // initilize 1wire
    if (onewire_reset(ONEWIRE_PIN))
        printf("1Wire reset. Detected devices.\n");
    else
        printf("1Wire reset. No detected devices.\n");

    int count = 0;
    // 1wire search
    onewire_search_start(&onewire_ds);
    do {
        onewire_addr_t device_addr;

        device_addr = onewire_search_next(&onewire_ds, ONEWIRE_PIN);
        if (device_addr != ONEWIRE_NONE) {
            sensors[count++] = device_addr;
            printf ("\t0x%llx\n", device_addr);
        } else {
            static volatile TickType_t ticklast = 0;
            volatile TickType_t ticknow = xTaskGetTickCount();
            if ((ticknow - ticklast) >= 1000) {
                printf("Error searching the 1wire bus.\n");
                ticklast = ticknow;
                break;
            } else {
                vTaskDelay(10); // avoids the watchdog triggering // which we still get with a simple taskYIELD() call
            }
        }

        // app_main_control();

    } while(!onewire_ds.last_device_found && (count <= MAX_SENSORS));
    printf("1Wire search: found %d devices.\n", count);

    // initialize the DS18B20 library
    ds18b20_init();

    // turn on green LED
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GREEN_LED, 1);

    // turn on yellow LED
    gpio_set_direction(YELLOW_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(YELLOW_LED, 1);

    // turn on red LED
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED, 1);

    // turn on U/V LED 1
    gpio_set_direction(UV1_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(UV1_LED, 1);

     // turn on U/V LED 2
    gpio_set_direction(UV2_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(UV2_LED, 1);

    // Initialise global xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount ();

    // Initial heater state
    heater.state = IDLE;
    heater.temperature = get_temperature();
    heater.setpoint = 74.0;
    heater.histeresis = 0.5;
    heater.adjustment = 5.0;

    // to kick the action at startup -- eventually triggered by user events
    if (heater.temperature < (heater.setpoint - heater.adjustment)) {
        // switch to heating mode
        HEATER_ON();
        heater.state = HEATING;
    } else {
        // switch to cooling mode
        HEATER_OFF();
        heater.state = COOLING;
    }

    while (1) {
  
#if 0
        // heater on
        int hcount = 0;
        heater_set(LEDC_MAX_DUTY);
        printf("[%d] Heater ON ---------\n", xTaskGetTickCount ());
        vTaskDelay(250 / portTICK_RATE_MS);

        // fan off
        gpio_set_direction(FAN_CTRL, GPIO_MODE_OUTPUT);
        gpio_set_level(FAN_CTRL, 0);

        // stabilize 1wire after heater or fan switch
        ds18b20_get_temp(sensors[0]);ds18b20_get_temp(sensors[1]);ds18b20_get_temp(sensors[2]);
     

        /* Simple "slightly busy" loop where we sleep to allow the IDLE task to
           execute so that it can tickle its watchdog. */
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[0], ds18b20_get_temp(sensors[0]));
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[1], ds18b20_get_temp(sensors[1]));
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[2], ds18b20_get_temp(sensors[2]));

        uint32_t in1;
        uint32_t in2;
        esp_err_t err = adc122s021_read(opamp_adc,&in1,&in2);
        if (ESP_OK == err) {
            ESP_LOGI(p_tag,"IN1 0x%08X IN2 0x%08X",in1,in2);
        }
        
        // heat for 30 seconds
        if (hcount++ == 30) {
            heater_set(0);
            printf("[%d] Heater OFF ---------\n", xTaskGetTickCount ());

            // fan on
            gpio_set_level(FAN_CTRL, 1);

            // stabilize 1wire
            ds18b20_get_temp(sensors[0]);ds18b20_get_temp(sensors[1]);ds18b20_get_temp(sensors[2]);
    //           onewire_reset(ONEWIRE_PIN);
        }
#endif
        app_main_control();
    }

    // This point should not be reached normally:
    esp_restart();
    return;
}

//=============================================================================
// EOF mlab100.c
