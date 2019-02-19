// mlab_webserver.h
//=============================================================================

#pragma once

//-----------------------------------------------------------------------------

#include "mlab.h"

#include <esp_https_server.h>

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------

/**
 * Start a HTTPS daemon.
 *
 * @param p_ctx Internal application private context pointer.
 * @return Handle onto server instance.
 */
httpd_handle_t webserver_start(void *p_ctx);

/**
 * Stop an active HTTPS daemon.
 *
 * @param server Handle returned from webserver_start().
 * @param p_ctx Pointer to application private context.
 */
void webserver_stop(httpd_handle_t server,void *p_ctx);

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
} // extern "C"
#endif // __cplusplus

//=============================================================================
// EOF mlab_webserver.h
