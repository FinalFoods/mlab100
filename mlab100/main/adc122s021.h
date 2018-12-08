// adc122s021.h
//=============================================================================

#if !defined(__adc122s021_h)
#define __adc122s021_h (1)

//-----------------------------------------------------------------------------

#include <inttypes.h>

#include "esp_types.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/spi_master.h"

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------
/**
 * Initialise the SPI bus and enumerate the ADC122S021 device.
 *
 * @return Handle onto ADC122S021 device.
 */

extern spi_device_handle_t app_init_spi(void);

/**
 * Read the ADC122S021 channels as (left-aligned) 32-bit samples.
 *
 * @param spi Handle onto SPI device.
 * @param p_in1 Pointer to field to be filled with channel IN1 value, or NULL if not required.
 * @param p_in2 Pointer to field to be filled with channel IN2 value, or NULL if not required.
 * @return ESP_OK on success, or standard esp-idf error encoding.
 */
extern esp_err_t adc122s021_read(spi_device_handle_t spi,uint32_t *p_in1,uint32_t *p_in2);

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif // __cplusplus

//-----------------------------------------------------------------------------

#endif // !__adc122s021_h

//=============================================================================
// EOF adc122s021.h
