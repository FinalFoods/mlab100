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

//-----------------------------------------------------------------------------

static const char *p_tag = "mblMain"; // esp-idf logging prefix (tag) for this module

static const char *p_owner = "Microbiota Labs"; // Project owner
static const char *p_model = MLAB_MODEL; // H/W model identifier
static const char *p_version = STRINGIFY(MLAB_VERSION); // F/W version identifier

void app_main(void)
{
    ESP_LOGI(p_tag,"Application starting " STRINGIFY(MLAB_VERSION) " (BUILDTIMESTAMP %" PRIX64 ")",(uint64_t)BUILDTIMESTAMP);

    printf("--------------------------------------\n");
    printf(" %s: Starting MLAB%s - version: %s\n", p_owner, p_model, p_version);
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
            ESP_LOGI(p_tag,"Default base MAC %02X:%02X:%02X:%02X:%02X:%02X",defmac[0],defmac[1],defmac[2],defmac[3],defmac[4],defmac[5]);
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

    spi_device_handle_t opamp_adc = app_init_spi();
    ESP_LOGD(p_tag,"opamp_adc %p",opamp_adc);

    // initialize the heater
    heater_init();

    // turn on 3.3V
    gpio_set_direction(CONTROL_3V3, GPIO_MODE_OUTPUT);
    gpio_set_level(CONTROL_3V3, 1);

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
       
    while (1) {
        printf("Temperature: %0.1f °C\n", ds18b20_get_temp());
        /* Simple "slightly busy" loop where we sleep to allow the IDLE task to
           execute so that it can tickle its watchdog. */
        vTaskDelay(1000 / portTICK_PERIOD_MS); // this avoids the IDLE watchdog triggering

#if 0 // simple test
        uint32_t in1;
        uint32_t in2;
        esp_err_t err = adc122s021_read(opamp_adc,&in1,&in2);
        if (ESP_OK == err) {
            ESP_LOGI(p_tag,"IN1 0x%08X IN2 0x%08X",in1,in2);
        }
#endif // boolean
    }

    // This point should not be reached normally:
    esp_restart();
    return;
}

//=============================================================================
// EOF mlab100.c
