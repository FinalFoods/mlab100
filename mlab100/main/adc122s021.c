// adc122s021.c
//=============================================================================
/* ADC122S021 2-channel 12-bit

   Compatible parts:
   - ADC128S052 8-channel
   - ADC124S021 4-channel
*/

//=============================================================================

#include "adc122s021.h"
#include <string.h>

//-----------------------------------------------------------------------------

static const char *p_tag = "adc"; // for esp-idf logging

//=============================================================================
#define PIN_NUM_MISO (19) // IC4#7 Dout
#define PIN_NUM_MOSI (23) // IC4#6 Din
#define PIN_NUM_CLK (18) // IC4#8 SCLK
#define PIN_NUM_CS (5) // IC4#1 nCS

//-----------------------------------------------------------------------------
/* Unfortunate use of globals, but keeps this code simple at the moment */

static spi_bus_config_t spi_buscfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
//    .max_transfer_sz = 4
    .max_transfer_sz = 8
};

static spi_device_interface_config_t spi_devcfg = {
    .clock_speed_hz = (1*1000*1000), // Clock out at 1MHz
    .mode = 0, // SPI mode 0
    .spics_io_num = PIN_NUM_CS, // nCS pin
    .queue_size = 2 // Number of transactions we wish to queue
};

//-----------------------------------------------------------------------------

spi_device_handle_t app_init_spi(void)
{
    spi_device_handle_t spi = 0;
    esp_err_t ret;

    ESP_LOGD(p_tag,"ADC122S021 initialisation");

    /* NOTE: For a *real* application we would handle the error cases
       explicitly, but for the moment we just use the esp-idf "example" of
       reporting the error. */

    // Initialize the SPI bus
    ret = spi_bus_initialize(HSPI_HOST,&spi_buscfg,1);
    ESP_ERROR_CHECK(ret);

    // Attach the ADC to the SPI bus
    ret = spi_bus_add_device(HSPI_HOST,&spi_devcfg,&spi);
    ESP_ERROR_CHECK(ret);

    ESP_LOGD(p_tag,"ADC122S021 device handle (SPI) %p",spi);

    return spi;
}

//=============================================================================
/* The following is the ADC122S021 specific handler */

#define REG_CTRL_ADD_SHIFT (3)
#define REG_CTRL_ADD_BITS  (3)
#define REG_CTRL_ADD_MASK  (((1 << REG_CTRL_ADD_BITS) - 1) << REG_CTRL_ADD_SHIFT)
#define REG_CTRL_ADD(_n)   (((_n) << REG_CTRL_ADD_SHIFT) & REG_CTRL_ADD_MASK)

#define CHANNEL_IN1 (0x0) // Default
#define CHANNEL_IN2 (0x1)
// All other channel numbers are "Not Allowed"

//-----------------------------------------------------------------------------

static esp_err_t adc_conversion(spi_device_handle_t spi,uint8_t channel,uint32_t *p_result)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t t;

    memset(&t,'\0',sizeof(t));

    // assert channel 0 (IN1) or 1 (IN2) .. all other values are undefined

    /* If we set SPI_TRANS_USE_TXDATA/RXDATA then we have 4-byte tx_data[] and
       rx_data[] buffers already in the spi_transaction_t structure to avoid a
       pointer de-reference. */
    t.flags = (SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA);
    // TODO:ASCERTAIN: Can we do this (TX and RX in same "transmit()" call?
    // We may have to do a separate read transaction to get the 2-bytes of ADC data
    t.length = (8 + 8);// (2 + 2);
    t.rxlength = 2;
    t.tx_data[0] = REG_CTRL_ADD(channel);
//    t.tx_data[0] = 0x20;
    t.tx_data[1] = 0x00;
    t.tx_data[2] = 0x00;
    t.tx_data[3] = 0x00;
  

    t.user = NULL;

    // mutex lock
    {
        /* For the moment we use the synchronous polling SPI transfer rather
           than doing an interrupt driven transfer. This can be revisited as the
           application requirements are refined. */ 
        ret = spi_device_polling_transmit(spi,&t);
    }
    // mutex unlock

    assert(ESP_OK == ret);

    if (p_result) {
        uint32_t result = ((t.rx_data[0] << 24) | ((t.rx_data[1] & 0xF) << 20));
        *p_result = result;
    }

    return ret;
}

//-----------------------------------------------------------------------------
/* On the rev??? (181201_0250) schematic

   IC4#5 IN1 LM6482#Aout
   IC4#4 IN2 LM6482#Bout
*/

esp_err_t adc122s021_read(spi_device_handle_t spi,uint32_t *p_in1,uint32_t *p_in2)
{
    esp_err_t ret = ESP_OK;

    if (p_in1) {
        ret = adc_conversion(spi,CHANNEL_IN1,p_in1);
        if (ESP_OK == ret) {
            ESP_LOGI(p_tag,"Read IN1 0x%08X",*p_in1);
        }
    }
    if ((ESP_OK == ret) && p_in2) {
        ret = adc_conversion(spi,CHANNEL_IN2,p_in2);
        if (ESP_OK == ret) {
            ESP_LOGI(p_tag,"Read IN2 0x%08X",*p_in2);
        }
    }

    if (ESP_OK != ret) {
        ESP_LOGE(p_tag,"Failed to read (%s)",esp_err_to_name(ret));
    }

    return ret;
}

//=============================================================================
// EOF adc122s021.c
