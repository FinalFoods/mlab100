// mlab_wifi.h
//=============================================================================

#pragma once

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "tcpip_adapter.h"

#if !defined(ETH_ALEN)
# define ETH_ALEN (6)
#endif // !ETH_ALEN

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------
// Application API

/**
 * Initialise the WiFi networking world and attach as a STA if configured
 * appropriately.
 *
 * @param p_ctx Pointer to private context (or NULL if not required).
 * @return ESP_OK on success, otherwise error indication.
 */
esp_err_t wifi_initialise(void *p_ctx);

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
} // extern "C"
#endif // __cplusplus

//=============================================================================
// EOF mlab_wifi.h
