// mlab_blufi.h
//=============================================================================

#pragma once

#include "mlab.h"

//-----------------------------------------------------------------------------

#if CONFIG_BT_ENABLED

//-----------------------------------------------------------------------------

#include "mlab_wifi.h"
#include "esp_bt.h"

#include "esp_blufi_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

//-----------------------------------------------------------------------------

#define BLUFI_TAG "mlabBluFi"

// The esp-idf example code uses wrappers for the logging:
#define BLUFI_INFO(fmt, ...)  ESP_LOGI(BLUFI_TAG,fmt,##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...) ESP_LOGE(BLUFI_TAG,fmt,##__VA_ARGS__)

//-----------------------------------------------------------------------------

/* Since we are using off-the-shelf ESP-WROOM-32D modules (with their factory
   assigned WiFi and BT MAC addresses) we just use their manufacturer ID instead
   of paying for our own allocation:
   https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers */
#define BLEADV_MANUF (0x02E5) // Manufacturer ID for ESP32 "Espressif" based hardware

/* After the required manufacturer identity we provide a Microbiota Labs
   specific "magic" ID value. */
#define BLEADV_MLAB (0x4C4D) // "ML" for Microbiota Labs

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------
// API between our WiFi and BluFi layers

/**
 * Notify BluFi world of STA connection to AP.
 *
 * @param connected Boolean true for connection, or false for disconnected.
 * @param bssid Vector containing BSSID of AP.
 * @param ssid_len Length of valid data from p_ssid.
 * @param p_ssid Pointer to SSID of ssid_len bytes.
 */
void blufi_connection(bool connected,uint8_t bssid[ETH_ALEN],uint8_t ssid_len,uint8_t *p_ssid);

/**
 * Notify BluFi world that the network connection is up (device has an IP address).
 *
 * @param mode Active WiFi mode.
 */
void blufi_wifi_sta_connected(wifi_mode_t mode);

/**
 * Notify BluFi world that the SoftAP has started.
 *
 * @param mode Active WiFi mode.
 */
void blufi_wifi_ap_start(wifi_mode_t mode);

//-----------------------------------------------------------------------------
// Standard BluFi API requirements

/**
 * Callback for data handler (DH) negotiation.
 *
 * @param p_data
 * @param len
 * @param pp_output_data
 * @param p_output_len
 * @param p_need_free
 */
void blufi_dh_negotiate_data_handler(uint8_t *p_data,int len,uint8_t **pp_output_data,int *p_output_len,bool *p_need_free);

/**
 * Encrypt data block in-situ.
 *
 * @param iv8 Initialisation vector value.
 * @param p_crypt_data Data buffer of at least crypt_len bytes.
 * @param crypt_len Amount of data to encrypt.
 * @return -1 on error, otherwise the passed crypt_len.
 */
int blufi_aes_encrypt(uint8_t iv8,uint8_t *p_crypt_data,int crypt_len);

/**
 * Decrypt data block in-situ.
 *
 * @param iv8 Initialisation vector value.
 * @param p_crypt_data Pointer to data buffer of at least crypt_len bytes.
 * @param crypt_len Amount of data to decrypt.
 * @return -1 on error, otherwise the passed crypt_len. 
*/
int blufi_aes_decrypt(uint8_t iv8,uint8_t *p_crypt_data,int crypt_len);

/**
 * Calculate the CRC16 for a block of data.
 *
 * @param iv8 Unused.
 * @param p_data Pointer to data over which CRC16 is to be calculated. 
 * @param len Number of bytes of valid data from p_data.
 * @return Big-endian CRC16.
 */
uint16_t blufi_crc_checksum(uint8_t iv8,uint8_t *p_data,int len);

/**
 * Initialise the BluFi security context.
 *
 * @return ESP_OK on success, otherwise error indication.
 */
esp_err_t blufi_security_init(void);

/**
 * Release any held security state.
 */
void blufi_security_deinit(void);

//-----------------------------------------------------------------------------
// Application API

/**
 * Initialise the BluFi world to allow BLE-GATT based WiFi configuration.
 * NOTE: The implementation expects the NVS and WiFi worlds to be have been
 * initialised prior to calling this function.
 *
 * @return ESP_OK on success, otherwise an error indication.
 */
esp_err_t mlab_blufi_start(void);

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
} // extern "C"
#endif // __cplusplus

//-----------------------------------------------------------------------------

#endif // CONFIG_BT_ENABLED

//=============================================================================
// EOF mlab_blufi.h
