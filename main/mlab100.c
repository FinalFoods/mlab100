/*
 * MLAB100 device application software.
 *
 * Copyright (C) 2019, Microbiota Labs Inc. and the MLAB100 contributors.
 *
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "mlab100.h"

static const char *p_tag = "Microbiota Labs";
static const char *p_model = MLAB_MODEL; // H/W model identifier
static const char *p_version = MLAB_VERSION; // F/W version identifier


void app_main()
{
    printf("--------------------------------------\n");
    printf(" Starting MLAB%s - version: %s\n", p_model, p_version);
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

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
