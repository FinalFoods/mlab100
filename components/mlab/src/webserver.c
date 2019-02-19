// webserver.c
//=============================================================================
/* Purely a very simple example HTTPS daemon server for the moment. Will need to
   extend and decide how we want to interface with the application.

   NOTEs:

   This webserver currently will NOT accept "http://" connections, only
   "https://". As such you will likely need to tell your browser to accept the
   unknown certificate. */
//=============================================================================

#include "mlab_webserver.h"

//-----------------------------------------------------------------------------

#if defined(CONFIG_MLAB_HTTPD) && CONFIG_MLAB_HTTPD

//-----------------------------------------------------------------------------

static const char * const p_tag = "httpd";

//-----------------------------------------------------------------------------

extern const uint8_t cacert_pem_start[] asm("_binary_cacert_pem_start");
extern const uint8_t cacert_pem_end[] asm("_binary_cacert_pem_end");
extern const uint8_t prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");

//-----------------------------------------------------------------------------

static esp_err_t root_get_handler(httpd_req_t *p_req)
{
    httpd_resp_set_type(p_req,"text/html");
    httpd_resp_send(p_req,"<h1>Hello HTTPS World!</h1>",-1); // -1 = use strlen()
    return ESP_OK;
}

//-----------------------------------------------------------------------------

static const httpd_uri_t root = {
    .uri     = "/",
    .method  = HTTP_GET,
    .handler = root_get_handler
};

//-----------------------------------------------------------------------------

httpd_handle_t webserver_start(void *p_ctx __attribute__((unused)))
{
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(p_tag,"Starting HTTPD server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(p_tag,"Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(p_tag,"Registering URI handlers");
    httpd_register_uri_handler(server,&root);

    return server;
}

//-----------------------------------------------------------------------------

void webserver_stop(httpd_handle_t server,void *p_ctx __attribute((unused)))
{
    // Stop the httpd server
    httpd_ssl_stop(server);
    return;
}

//-----------------------------------------------------------------------------

#endif // CONFIG_MLAB_HTTPD

//=============================================================================
// EOF webserver.c
