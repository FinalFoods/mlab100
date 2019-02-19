// mlab.h
//==============================================================================

//==============================================================================

#pragma once

//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "esp_types.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event_loop.h"
#include "soc/efuse_reg.h"

#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "mbedtls/x509_crt.h"

//-----------------------------------------------------------------------------

#if !defined(STRINGIFY)
# define STRINGIFY(_x) STRINGIFY2(_x)
# define STRINGIFY2(_x) #_x
#endif // !STRINGIFY

#if !defined(NUMOF)
# define NUMOF(t) (sizeof(t) / sizeof(t[0]))
#endif // !NUMOF

// BIT(x) defined by "soc/soc.h"
#if !defined(BMASK)
# define BMASK(__n,__s)  (((1 << (__s)) - 1) << (__n))
#endif // !BMASK

#if !defined(BVALUE)
# define BVALUE(__n,__v) ((__v) << (__n))
#endif // !BVALUE

#if !defined(BVALUE_GET)
# define BVALUE_GET(__n,__s,__v) (((__v) >> (__n)) & BMASK(0,__s))
#endif // !BVALUE_GET

#define POW2(__n) (1ULL << (__n))

//-----------------------------------------------------------------------------

// Shorthand to save typing when printing MAC addresses (e.g. BSSID values):
#define PRIXMAC "%02X:%02X:%02X:%02X:%02X:%02X"
#define PRINTMAC(m) (m)[0],(m)[1],(m)[2],(m)[3],(m)[4],(m)[5]

//-----------------------------------------------------------------------------

extern EventGroupHandle_t eventgroup_app; /**< Global application event flags. */

#if (configUSE_16_BIT_TICKS == 0)
// 24 event flags per event group
#elif (configUSE_16_BIT_TICKS == 1)
// 8 event flags per event group
# error "We require a configuration with 24-bit event group (configUSE_16_BIT_TICKS==0)"
#else // unknown
# error "Unknown configUSE_16_BIT_TICKS setting"
#endif // unknown

#define EG_WIFI_CONNECTED (BIT0) // WiFi STA has IP address
// All other valid bit positions are currently undefined.

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------

// NUL-terminated strings provided by the main application:
const char * const p_owner;
const char * const p_model;
const char * const p_version;
const char * const p_device;

//-----------------------------------------------------------------------------

/**
 * Common platform initialisation shared between firmware applications.
 */
void app_common_platform_init(void);

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif // __cplusplus

//==============================================================================
// EOF mlab.h
