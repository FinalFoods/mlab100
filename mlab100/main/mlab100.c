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

//-----------------------------------------------------------------------------

static const char * const p_tag = "mblMain"; // esp-idf logging prefix (tag) for this module

const char * const p_owner = "Microbiota Labs"; // Project owner
const char * const p_model = MLAB_MODEL; // H/W model identifier
const char * const p_version = STRINGIFY(MLAB_VERSION); // F/W version identifier
const char * const p_device = "MLAB" MLAB_MODEL; // Human-readable device name

static onewire_search_t onewire_ds;
#define MAX_SENSORS 3
static onewire_addr_t sensors[MAX_SENSORS];

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

    app_common_platform_init();
#if defined(CONFIG_MLAB_BLUFI) && CONFIG_MLAB_BLUFI
    if (ESP_OK == wifi_initialise()) {
        if (ESP_OK != mlab_blufi_start()) {
            ESP_LOGE(p_tag,"Failed to start BluFi");
        }
    } else {
        ESP_LOGE(p_tag,"Failed to initialise WiFi");
    }
#endif // CONFIG_MLAB_BLUFI

    spi_device_handle_t opamp_adc = app_init_spi();
    ESP_LOGD(p_tag,"opamp_adc %p",opamp_adc);
 
    // initialize GPIO output pins -- onewire is setup by its own library
    #define GPIO_OUTPUT_PIN_SEL  ((1ULL<<CONTROL_3V3) | (1ULL<<GREEN_LED) | (1ULL<<YELLOW_LED) | (1ULL<<RED_LED) | (1ULL<<UV1_LED) | (1ULL<<UV2_LED) )

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

    gpio_pad_select_gpio(GREEN_LED);
    gpio_pad_select_gpio(YELLOW_LED);
    // gpio_pad_select_gpio(RED_LED);
    gpio_pad_select_gpio(CONTROL_3V3);


   // initialize the heater
    heater_init();

    // turn on 3.3V
    gpio_set_direction(CONTROL_3V3, GPIO_MODE_OUTPUT);
    gpio_set_level(CONTROL_3V3, 1);

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
        } else
            printf("Error searching the 1wire bus.\n");

    } while(!onewire_ds.last_device_found && (count <= MAX_SENSORS));
    printf("1Wire search: found %d devices.\n", count);

    // initialize the DS18B20 library
    ds18b20_init();

    // turn on green LED
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GREEN_LED, 0);

    // turn on yellow LED
    gpio_set_direction(YELLOW_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(YELLOW_LED, 0);

    // turn on red LED
    // gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED, 0);

    // turn on U/V LED 1
    gpio_set_direction(UV1_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(UV1_LED, 1);

     // turn on U/V LED 2
    gpio_set_direction(UV2_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(UV2_LED, 1);

    int toggle = 1;
       
    while (1) {
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[0], ds18b20_get_temp(sensors[0]));
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[1], ds18b20_get_temp(sensors[1]));
        printf("Temperature[0x%llx]: %0.1f °C\n", sensors[2], ds18b20_get_temp(sensors[2]));
 
        /* Simple "slightly busy" loop where we sleep to allow the IDLE task to
           execute so that it can tickle its watchdog. */
        vTaskDelay(1000 / portTICK_PERIOD_MS); // this avoids the IDLE watchdog triggering

//#if 0 // simple test
        // turn on U/V LED 1
        gpio_set_direction(UV1_LED, GPIO_MODE_OUTPUT);
        gpio_set_level(UV1_LED, toggle);

        // turn on U/V LED 2
        gpio_set_direction(UV2_LED, GPIO_MODE_OUTPUT);
        gpio_set_level(UV2_LED, toggle);

        uint32_t in1;
        uint32_t in2;
        esp_err_t err = adc122s021_read(opamp_adc,&in1,&in2);
        if (ESP_OK == err) {
            ESP_LOGI(p_tag,"IN1 0x%08X IN2 0x%08X",in1,in2);
        }
        
        if (toggle) 
            toggle = 0;
        else
            toggle = 1;

//#endif // boolean
    }

    // This point should not be reached normally:
    esp_restart();
    return;
}

//=============================================================================
// EOF mlab100.c
