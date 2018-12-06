/*
 * MLAB100 device application software.
 *
 * Copyright (C) 2019, Microbiota Labs Inc. and the MLAB100 contributors.
 *
 */

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

#include "mlab100.h"

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

    /* TODO:IMPLEMENT: Either our main loop or code to create the necessary
       threads for whatever loops we want. */
    while (1) {
        /* Simple "slightly busy" loop where we sleep to allow the IDLE task to
           execute so that it can tickle its watchdog. */
        vTaskDelay(1000 / portTICK_PERIOD_MS); // this avoids the IDLE watchdog triggering
    }

    // This point should not be reached normally:
    esp_restart();
    return;
}

//=============================================================================
// EOF mlab100.c
