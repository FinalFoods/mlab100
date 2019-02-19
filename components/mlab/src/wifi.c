// wifi.c
//=============================================================================
/* We hold the common/shared WiFi support in this component library to allow the
   functionality to be shared across firmware projects. */
//-----------------------------------------------------------------------------
/* WARNING: There is a BUG in the esp-idf implementations that they expect a
   NUL-terminated SSID in a 32-byte vector which means that valid 32-character
   SSIDs may not be handled correctly. */
//=============================================================================

#include "mlab_blufi.h"

#if !defined(DEFAULT_RSSI)
# define DEFAULT_RSSI (-127)
#endif // !DEFAULT_RSSI

//#define CONFIG_MLAB_SOFTAP (1) // define to enable SoftAP mode // TODO:CONSIDER: can be a sdkconfig menuconfig controlled option if we want

#define DO_EXTRADBG (1) // define to enable some extra diagnostics

//-----------------------------------------------------------------------------

static const char * const p_tag = "WiFi";

//-----------------------------------------------------------------------------

/* This is the esp-idf network state event handler. That implementation is
   basically just a hardwired SINGLE attached callback to an explicitly created
   QueueHandle_t queue thread. */
static esp_err_t event_handler(void *p_ctx,system_event_t *p_event)
{
    if (NULL == p_event) {
        ESP_LOGE(p_tag,"NULL event");
        return ESP_FAIL; // generic error
    }

    switch (p_event->event_id) {
    case SYSTEM_EVENT_STA_START: /**< ESP32 STAtion start */
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP: /**< ESP32 STAtion got IP via AP association. */
        {
            wifi_mode_t mode;

            xEventGroupSetBits(eventgroup_app,EG_WIFI_CONNECTED);
            esp_wifi_get_mode(&mode);
#if CONFIG_MLAB_BLUFI
            blufi_wifi_sta_connected(mode);
#endif // CONFIG_MLAB_BLUFI
        }
        break;

    case SYSTEM_EVENT_STA_CONNECTED: /**< ESP32 STAtion associated with AP. */
#if defined(DO_EXTRADBG)
        {
            system_event_sta_connected_t *p_connected = &p_event->event_info.connected;

            /* This is a work around for the BROKEN esp-idf world we are
               currently using since SSIDs can validly be 32-chars long and they
               do not set aside enough space. */

            uint8_t ssid_copy[32 + 1]; // space for terminating NUL for string output
            memset(ssid_copy,'\0',sizeof(ssid_copy)); // will force terminating NUL
            memcpy(ssid_copy,p_connected->ssid,p_connected->ssid_len);
            ESP_LOGI(p_tag,"STA connected: SSID: \"%s\" BSSID " PRIXMAC " channel %d authmode %d",ssid_copy,PRINTMAC(p_connected->bssid),p_connected->channel,p_connected->authmode);
        }
#endif // DO_EXTRADBG

#if CONFIG_MLAB_BLUFI
        blufi_connection(true,p_event->event_info.connected.bssid,p_event->event_info.connected.ssid_len,p_event->event_info.connected.ssid);
#endif // CONFIG_MLAB_BLUFI
        break; 

    case SYSTEM_EVENT_STA_DISCONNECTED: /**< ESP32 STAtion disconnected from AP. */
        /* This is a workaround as ESP32 WiFi libraries do not currently
           auto-reassociate. */
#if CONFIG_MLAB_BLUFI
        blufi_connection(false,NULL,0,NULL);
#endif // CONFIG_MLAB_BLUFI

        esp_wifi_connect();
        xEventGroupClearBits(eventgroup_app,EG_WIFI_CONNECTED);
        break;

    case SYSTEM_EVENT_AP_START: /**< ESP32 soft-AP start. */
        {
            wifi_mode_t mode;

            esp_wifi_get_mode(&mode);

#if CONFIG_MLAB_BLUFI
            blufi_wifi_ap_start(mode);
#endif // CONFIG_MLAB_BLUFI
        }
        break;

    case SYSTEM_EVENT_SCAN_DONE: /**< ESP32 finish scanning AP. */
        {
            uint16_t apCount = 0;
            esp_err_t rcode;

            /* CONSIDER: Ideally this code should avoid the use of dynamic heap,
               at the expense of pre-allocating vectors of the required
               depths. */

            rcode = esp_wifi_scan_get_ap_num(&apCount);
            if ((ESP_OK == rcode) && apCount) {
                wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);

                if (NULL == ap_list) {
                    ESP_LOGE(p_tag,"OOM: ap_list is NULL");
                    rcode = ESP_ERR_NO_MEM;
                } else {
                    rcode = esp_wifi_scan_get_ap_records(&apCount,ap_list);
                    if (ESP_OK == rcode) {
#if CONFIG_MLAB_BLUFI
                        esp_blufi_ap_record_t *blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
                        if (NULL == blufi_ap_list) {
                            BLUFI_ERROR("OOM, blufi_ap_list is NULL");
                            rcode = ESP_ERR_NO_MEM;
                        } else {
                            for (unsigned int i = 0; (i < apCount); ++i) {
                                blufi_ap_list[i].rssi = ap_list[i].rssi;
                                memcpy(blufi_ap_list[i].ssid,ap_list[i].ssid,sizeof(ap_list[i].ssid));
                            }
                            esp_blufi_send_wifi_list(apCount,blufi_ap_list);
                            free(blufi_ap_list);
                        }
#endif // CONFIG_MLAB_BLUFI
                    } else {
                        ESP_LOGE(p_tag,"Failed to get AP records");
                    }

                    free(ap_list);
                }
            }

            if (ESP_OK == rcode) {
                rcode = esp_wifi_scan_stop();
            }

            if (ESP_OK != rcode) {
                ESP_LOGE(p_tag,"SYSTEM_EVENT_SCAN_DONE error (%d) \"%s\"",rcode,esp_err_to_name(rcode));
            }
        }
        break;

    case SYSTEM_EVENT_WIFI_READY: /**< ESP32 WiFi ready */
    case SYSTEM_EVENT_STA_STOP: /**< ESP32 station stop */
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected by ESP32 station changed */
    case SYSTEM_EVENT_STA_LOST_IP: /**< ESP32 station lost IP and the IP is reset to 0 */
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS: /**< ESP32 station wps succeeds in enrollee mode */
    case SYSTEM_EVENT_STA_WPS_ER_FAILED: /**< ESP32 station wps fails in enrollee mode */
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT: /**< ESP32 station wps timeout in enrollee mode */
    case SYSTEM_EVENT_STA_WPS_ER_PIN: /**< ESP32 station wps pin code in enrollee mode */
    case SYSTEM_EVENT_AP_STOP: /**< ESP32 soft-AP stop */
    case SYSTEM_EVENT_AP_STACONNECTED: /**< a station connected to ESP32 soft-AP */
        /* DECIDE: start SoftAP HTTPD server for configuration */
    case SYSTEM_EVENT_AP_STADISCONNECTED: /**< a station disconnected from ESP32 soft-AP */
        /* DECIDE: stop SoftAP HTTPD server for configuration */
    case SYSTEM_EVENT_AP_PROBEREQRECVED: /**< Receive probe request packet in soft-AP interface */
    case SYSTEM_EVENT_GOT_IP6: /**< ESP32 station or ap or ethernet interface v6IP addr is preferred */
    case SYSTEM_EVENT_ETH_START: /**< ESP32 ethernet start */
    case SYSTEM_EVENT_ETH_STOP: /**< ESP32 ethernet stop */
    case SYSTEM_EVENT_ETH_CONNECTED: /**< ESP32 ethernet phy link up */
    case SYSTEM_EVENT_ETH_DISCONNECTED: /**< ESP32 ethernet phy link down */
    case SYSTEM_EVENT_ETH_GOT_IP: /**< ESP32 ethernet got IP from connected AP */
    default:
        break;
    }

    /* If needed we could fill in a pointer above, and when non-NULL trigger
       some NON-BLOCKING application synchronisation
       here. e.g. xQueueSendToBack(queue,ptr,0) to deliver the event to a
       waiting thread. */

    return ESP_OK;
}

//-----------------------------------------------------------------------------

esp_err_t wifi_initialise(void)
{
    esp_err_t result = ESP_OK;

    tcpip_adapter_init();

    result = esp_event_loop_init(event_handler,NULL);
    if (ESP_OK == result) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        result = esp_wifi_init(&cfg);
        if (ESP_OK == result) {

            /* cfg.nvs_enable reflects the sdkconfig CONFIG_ESP32_WIFI_NVS_ENABLED
               setting. We do NOT have the source to the esp_wifi_set_storage()
               function. */

            if (1 == cfg.nvs_enable) {
                ESP_LOGI(p_tag,"WIFI_STORAGE_FLASH");
                result = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
            } else {
                ESP_LOGI(p_tag,"WIFI_STORAGE_RAM");
                result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
            }
        }
    }

    /* From: http://esp-idf.readthedocs.io/en/latest/api-guides/wifi.html

       If the Wi-Fi NVS flash is enabled by menuconfig, all Wi-Fi configuration
       in this phase, or later phases, will be stored into flash. When the board
       powers on/reboots, you do not need to configure the Wi-Fi driver from
       scratch. You only need to call esp_wifi_get_xxx APIs to fetch the
       configuration stored in flash previously. You can also configure the
       Wi-Fi driver if the previous configuration is not what you want. */

#if defined(DO_EXTRADBG)
    {
        uint8_t mac[ETH_ALEN];

        if (ESP_OK == esp_wifi_get_mac(WIFI_IF_STA,mac)) {
            ESP_LOGI(p_tag,"STA MAC " PRIXMAC "",PRINTMAC(mac));
        } else {
            ESP_LOGE(p_tag,"Failed to get STA MAC");
        }

        if (ESP_OK == esp_wifi_get_mac(WIFI_IF_AP,mac)) {
            ESP_LOGI(p_tag," AP MAC " PRIXMAC "",PRINTMAC(mac));
        } else {
            ESP_LOGE(p_tag,"Failed to get AP MAC");
        }
    }
#endif // DO_EXTRADBG

    if (ESP_OK == result) {
        wifi_config_t cfg_stored;

        /* We check whether we have ANY stored configuration at this point. If
           we have NO initialsed NVS configuration then we force the factory
           defaults. */

        result = esp_wifi_get_config(WIFI_IF_STA,&cfg_stored);
        if (ESP_OK == result) {
            if (cfg_stored.sta.bssid_set) {
                ESP_LOGW(p_tag,"stored cfg: SSID \"%s\" password \"%s\" BSSID " PRIXMAC "",cfg_stored.sta.ssid,cfg_stored.sta.password,PRINTMAC(cfg_stored.sta.bssid));
            } else {
                ESP_LOGI(p_tag,"stored cfg: SSID \"%s\" password \"%s\" bssid_set FALSE",cfg_stored.sta.ssid,cfg_stored.sta.password);
            }

#if defined(DO_EXTRADBG)
            ESP_LOGI(p_tag,"stored cfg: scan_method %s",((WIFI_FAST_SCAN == cfg_stored.sta.scan_method) ? "WIFI_FAST_SCAN" : ((WIFI_ALL_CHANNEL_SCAN == cfg_stored.sta.scan_method) ? "WIFI_ALL_CHANNEL_SCAN" : "<UNKNOWN>")));
            ESP_LOGI(p_tag,"stored_cfg: channel %u",cfg_stored.sta.channel);
            ESP_LOGI(p_tag,"stored_cfg: sort_method %s",((WIFI_CONNECT_AP_BY_SIGNAL == cfg_stored.sta.sort_method) ? "WIFI_CONNECT_AP_BY_SIGNAL" : ((WIFI_CONNECT_AP_BY_SECURITY == cfg_stored.sta.sort_method) ? "WIFI_CONNECT_AP_BY_SECURITY" : "<UNKNOWN>")));
            ESP_LOGI(p_tag,"stored_cfg: threshold: rssi %d",cfg_stored.sta.threshold.rssi);
            {
                char *p_authmode;
                switch (cfg_stored.sta.threshold.authmode) {
                case WIFI_AUTH_OPEN: p_authmode = "WIFI_AUTH_OPEN"; break;
                case WIFI_AUTH_WEP: p_authmode = "WIFI_AUTH_WEP"; break;
                case WIFI_AUTH_WPA_PSK: p_authmode = "WIFI_AUTH_WPA_PSK"; break;
                case WIFI_AUTH_WPA2_PSK: p_authmode = "WIFI_AUTH_WPA2_PSK"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: p_authmode = "WIFI_AUTH_WPA_WPA2_PSK"; break;
                case WIFI_AUTH_WPA2_ENTERPRISE: p_authmode = "WIFI_AUTH_WPA2_ENTERPRISE"; break;
                default: p_authmode = "<UNKNOWN>"; break;
                }
                ESP_LOGI(p_tag,"stored_cfg: threshold: authmode %s",p_authmode);
            }
#endif // DO_EXTRADBG

            /* NASTY Espressif use of NUL-terminated strings: */
            if (('\0' == cfg_stored.sta.ssid[0]) && ('\0' == cfg_stored.sta.password[0])) {
                /* This is only reached immediately after an erase_flash or a user
                   triggered factory-reset. */
                ESP_LOGW(p_tag,"First execution: Setting factory defaults");
                /* We could set explicit SSID/PSK factory defaults at this
                   point if we wanted, but that is not likely to be the case
                   for "consumer" product that is not going to live in a
                   known network space. */
                memset(cfg_stored.sta.ssid,'\0',sizeof(cfg_stored.sta.ssid));
                memset(cfg_stored.sta.password,'\0',sizeof(cfg_stored.sta.password));
                cfg_stored.sta.scan_method = WIFI_FAST_SCAN;
                cfg_stored.sta.bssid_set = false;
                memset(cfg_stored.sta.bssid,'\0',sizeof(cfg_stored.sta.bssid));
                cfg_stored.sta.channel = 0; // scan 1..N
                cfg_stored.sta.sort_method = WIFI_CONNECT_AP_BY_SECURITY;
                cfg_stored.sta.threshold.rssi = DEFAULT_RSSI;
                ESP_LOGI(p_tag,"Setting DEFAULT_RSSI %d",cfg_stored.sta.threshold.rssi);
                cfg_stored.sta.threshold.authmode = WIFI_AUTH_OPEN;
            }

            /* We attempt to connect to the configured STA Access Point. We do not care
               if this is the factory AP or a "user" configuration. */
            if (cfg_stored.sta.bssid_set) {
                ESP_LOGW(p_tag,"Setting WiFi configuration SSID %s BSSID " PRIXMAC "",cfg_stored.sta.ssid,PRINTMAC(cfg_stored.sta.bssid));
            } else {
                ESP_LOGI(p_tag,"Setting WiFi configuration SSID %s",cfg_stored.sta.ssid);
            }

            result = esp_wifi_set_mode(WIFI_MODE_STA);
            if (ESP_OK == result) {
                result = esp_wifi_set_config(ESP_IF_WIFI_STA,&cfg_stored);
            }
        }
    }

#if defined(CONFIG_MLAB_WIFI_SECURITY_EAP_TLS) && CONFIG_MLAB_WIFI_SECURITY_EAP_TLS
    // This is an example of configuring for connection to a 802.1X EAP-TLS AP:
    if (ESP_OK == result) {
        // Need valid certificates and working ESP32 EAP-TLS run-time:
        unsigned int ca_pem_bytes = (unsigned int)(ca_pem_end - ca_pem_start);
        unsigned int client_crt_bytes = (unsigned int)(client_crt_end - client_crt_start);
        unsigned int client_key_bytes = (unsigned int)(client_key_end - client_key_start);
        esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();

        result = esp_wifi_sta_wpa2_ent_set_ca_cert(ca_pem_start,ca_pem_bytes);
        if (ESP_OK == result) {
            result = esp_wifi_sta_wpa2_ent_set_cert_key(client_crt_start,client_crt_bytes,client_key_start,client_key_bytes,NULL,0);
        }
        if (ESP_OK == result) {
            result = esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)CONFIG_EAP_ID,strlen(CONFIG_EAP_ID));
        }
        if (ESP_OK == result) {
            result = esp_wifi_sta_wpa2_ent_enable(&config);
        }
    }
#endif // CONFIG_MLAB_WIFI_SECURITY_EAP_TLS

#if defined(CONFIG_MLAB_SOFTAP) && CONFIG_MLAB_SOFTAP
    // If we are providing a SoftAP:
    {
        wifi_config_t cfg_softap = {
            .ap = {
                .ssid = "local99", // TODO:IMPLEMENT: should be "mlab<MAC>" ideally
                .channel = 0,
                .authmode = WIFI_AUTH_OPEN, // TODO:CONSIDER: should maybe eventually be a WPA2-PSK using a PSK as printed on the box or similarly controlled to limit access
                .ssid_hidden = 0,
                .max_connection = 1,
                .beacon_interval = 100
            }
        };

        result = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ESP_OK == result) {
            result = esp_wifi_set_config(WIFI_IF_AP,&cfg_softap);
        }
    }
#endif // CONFIG_MLAB_SOFTAP

    if (ESP_OK == result) {
        result = esp_wifi_start();
    }

    if (ESP_OK != result) {
        ESP_LOGE(p_tag,"WiFi initialisation failed with error (%d) \"%s\"",result,esp_err_to_name(result));
    }

    return result;
}

//=============================================================================
// EOF wifi.c
