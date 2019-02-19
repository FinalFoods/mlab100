// blufi.c
//=============================================================================
/* Copied from esp-idf/examples/bluetooth/blufi/main/blufi_example_main.c and
   edited for use in another application rather than as a standalone
   example. The original source (prior to local Microbiota Labs modifications)
   was covered by the following license statement: */
/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
//=============================================================================

/* BluFi is the Espressif "standard" implementation for configuring the WiFi via
   a Bluetooth (BLE-GATT) connection. The ESP32 WiFi can be configured to
   connect to an AP, or to be configured as a SoftAP itself. */

/* The sources for the "standard" Espressif BluFi demonstration/example
   applications can be found at:
     Android: https://github.com/EspressifApp/EspBlufiForAndroid
         iOS: https://github.com/EspressifApp/EspBlufiForiOS

   For Microbiota Labs we have our own UI application which also provides the
   BluFi capabality to configure the WiFi as needed. */

//=============================================================================

#include "mlab_blufi.h"

//-----------------------------------------------------------------------------

#if CONFIG_BT_ENABLED
# if defined(CONFIG_MLAB_BLUFI) && CONFIG_MLAB_BLUFI

//-----------------------------------------------------------------------------

/* Doing Connect from "BLE Scanner" shows:
   Generic Attribute    0x1801 					Primary Service		UUID 00002A05-0000-1000-8000-00805F9B34FB	UUID 0x2902
   Generic Access       0x1800 					Primary Service		DEVICE NAME READ, APPEARANCE READ, CENTRAL ADDRESS RESOLUTION READ
   Custom Service       0000FFFF-0000-1000-8000-00805F9B34FB    Primary Service		CUSTOM CHARACTERISTIC WRITE (WRITE REQUEST), CUSTOM CHARACTERISTIC READ,NOTIFFY
*/
/* https://www.bluetooth.com/specifications/assigned-numbers/service-discovery

   BASE_UUID 00000000-0000-1000-8000-00805F9B34FB
*/
/* The EspBlufiForAndroid application allows for custom data to be posted to the device. e.g.:
	I (193713) mlabBluFi: event: 26
	I (193713) mlabBluFi: Recv Custom Data 5
        I (193713) Custom Data: 68 65 6c 6c 6f 
*/

static uint8_t blufi_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // first UUID, 16bit, [12],[13] is the value
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, // 0000FFFF-0000-1000-8000-00805F9B34FB
};

/* We detect the device for our BluFi operation based on the Manufacturer
   specific data. We use the Espressif manufacturer code and then our own
   (short) key to identify a Microbiota Labs device. Not perfect, but better
   than a string match on the name. */

static const uint8_t manufacturer_data[] =  {
    ((BLEADV_MANUF >> 0) & 0xFF),((BLEADV_MANUF >> 8) & 0xFF), // BLE Manufacturer code
    ((BLEADV_MLAB >> 0) & 0xFF), ((BLEADV_MLAB >> 8) & 0xFF), // Microbiota Labs "magic" marker
    /* If required we can provide a small amount of Microbiota Labs specific
       data at this point. e.g. a serial# or some other identifying
       information. */
};

//-----------------------------------------------------------------------------

static const esp_ble_adv_data_t blufi_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //  7.5ms // slave connection min interval, Time = (min_interval * 1.25 msec)
    .max_interval = 0x0010, // 20.0ms // slave connection max interval, Time = (max_interval * 1.25 msec)
    .appearance = 0x00,
    .manufacturer_len = (uint16_t)sizeof(manufacturer_data),
    .p_manufacturer_data =  (uint8_t *)manufacturer_data,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16, /* (uint16_t)sizeof(blufi_service_uuid128), */
    .p_service_uuid = (uint8_t *)blufi_service_uuid128,
    .flag = 0x6,
};

//-----------------------------------------------------------------------------

static esp_ble_adv_params_t adv_params = {
    .adv_int_min       = 0x100, // 160ms // Time = (N * 0.625) msec
    .adv_int_max       = 0x100, // 160ms // Time = (N * 0.625) msec
    .adv_type          = ADV_TYPE_IND, // "Advertising Indications" : peripheral requests connection to any central device (not directed at particular central device)
    .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr       =
    //.peer_addr_type  =
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

//-----------------------------------------------------------------------------

/* BLE connection information: */
static uint8_t server_if = 0; // TODO:VERIFY: That a valid server_if can never be 0
static uint16_t conn_id = 0;

//-----------------------------------------------------------------------------
// Unfortunate global state for tracking data between BluFi events:

/* We store the station information for sending to the BluFi connected application: */
static bool gl_sta_connected = false;
static uint8_t gl_sta_bssid[ETH_ALEN];
static uint8_t gl_sta_ssid[32 + 1]; // we allow for a terminating NUL
static int gl_sta_ssid_len;
static wifi_config_t sta_config;
static wifi_config_t ap_config;

//-----------------------------------------------------------------------------

void blufi_connection(bool connected,uint8_t bssid[ETH_ALEN],uint8_t ssid_len,uint8_t *p_ssid)
{
    gl_sta_connected = connected;

    if (true == gl_sta_connected) {
        memcpy(gl_sta_bssid,bssid,ETH_ALEN);
        memcpy(gl_sta_ssid,p_ssid,ssid_len);
        gl_sta_ssid_len = ssid_len;
    } else {
        memset(gl_sta_ssid,'\0',sizeof(gl_sta_ssid));
        memset(gl_sta_bssid,'\0',sizeof(gl_sta_bssid));
        gl_sta_ssid_len = 0;
    }

    return;
}

//-----------------------------------------------------------------------------

void blufi_wifi_sta_connected(wifi_mode_t mode)
{
    if (server_if) {
        esp_blufi_extra_info_t info;

        memset(&info,'\0',sizeof(esp_blufi_extra_info_t));

        memcpy(info.sta_bssid,gl_sta_bssid,ETH_ALEN);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;

        esp_blufi_send_wifi_conn_report(mode,ESP_BLUFI_STA_CONN_SUCCESS,0,&info);
    }

    return;
}

//-----------------------------------------------------------------------------

void blufi_wifi_ap_start(wifi_mode_t mode)
{
    if (server_if) {
        if (gl_sta_connected) {  
            esp_blufi_send_wifi_conn_report(mode,ESP_BLUFI_STA_CONN_SUCCESS,0,NULL);
        } else {
            esp_blufi_send_wifi_conn_report(mode,ESP_BLUFI_STA_CONN_FAIL,0,NULL);
        }
    }

    return;
}

//-----------------------------------------------------------------------------

static void blufi_event_callback(esp_blufi_cb_event_t event,esp_blufi_cb_param_t *p_param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    BLUFI_INFO("event: %d",event);

    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        BLUFI_INFO("BLUFI init finish");
        /* ASCERTAIN: When not using the raw support ascertain where they build
           the advertisement packet (since they do not seem to use the
           ESP_BLE_AD_TYPE_NAME_CMPL et-al manifests. Just so we can see if we
           can easily change the service UUID given etc. without changing the
           esp-idf sources. */
        esp_ble_gap_set_device_name(p_device); // application supplied name
        esp_ble_gap_config_adv_data((esp_ble_adv_data_t *)&blufi_adv_data);
        break;

    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        BLUFI_INFO("BLUFI deinit finish");
        break;

    case ESP_BLUFI_EVENT_BLE_CONNECT:
        server_if = p_param->connect.server_if;
        conn_id = p_param->connect.conn_id;
        BLUFI_INFO("BLUFI ble connect (server %02X conn %04X)",server_if,conn_id);
        esp_ble_gap_stop_advertising();
        blufi_security_init();
        break;

    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        BLUFI_INFO("BLUFI ble disconnect");
        blufi_security_deinit();
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        BLUFI_INFO("BLUFI Set WIFI opmode %d", p_param->wifi_mode.op_mode);
        {
            esp_err_t rcode = esp_wifi_set_mode(p_param->wifi_mode.op_mode);
            if (ESP_OK != rcode) {
                BLUFI_ERROR("ESP_BLUFI_EVENT_SET_WIFI_OPMODE set_mode error (0x%X) %s",rcode,esp_err_to_name(rcode));
            }
        }
        break;

    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        BLUFI_INFO("BLUFI requset wifi connect to AP");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
        esp_wifi_disconnect();
        esp_wifi_connect();
        break;

    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        BLUFI_INFO("BLUFI requset wifi disconnect from AP");
        esp_wifi_disconnect();
        break;

    case ESP_BLUFI_EVENT_REPORT_ERROR:
        BLUFI_ERROR("BLUFI report error, error code %d", p_param->report_error.state);
        esp_blufi_send_error_info(p_param->report_error.state);
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
        {
            wifi_mode_t mode;
            esp_blufi_extra_info_t info;

            esp_wifi_get_mode(&mode);

            if (gl_sta_connected) {  
                memset(&info,'\0',sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid,gl_sta_bssid,6);
                info.sta_bssid_set = true;
                info.sta_ssid = gl_sta_ssid;
                info.sta_ssid_len = gl_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode,ESP_BLUFI_STA_CONN_SUCCESS,0,&info);
            } else {
                esp_blufi_send_wifi_conn_report(mode,ESP_BLUFI_STA_CONN_FAIL,0,NULL);
            }

            BLUFI_INFO("BLUFI get wifi status from AP");
        }
        break;

    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        BLUFI_INFO("blufi close a gatt connection");
        esp_blufi_close(server_if,conn_id);
        conn_id = 0;
        server_if = 0;
        break;

    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;

    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, p_param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA BSSID %s", sta_config.sta.ssid);
        break;

    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)p_param->sta_ssid.ssid, p_param->sta_ssid.ssid_len);
        sta_config.sta.ssid[p_param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA SSID %s", sta_config.sta.ssid);
        break;

    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)p_param->sta_passwd.passwd, p_param->sta_passwd.passwd_len);
        sta_config.sta.password[p_param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        BLUFI_INFO("Recv STA PASSWORD %s", sta_config.sta.password);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)p_param->softap_ssid.ssid, p_param->softap_ssid.ssid_len);
        ap_config.ap.ssid[p_param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = p_param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP SSID %s, ssid len %d", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)p_param->softap_passwd.passwd, p_param->softap_passwd.passwd_len);
        ap_config.ap.password[p_param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP PASSWORD %s len = %d", ap_config.ap.password, p_param->softap_passwd.passwd_len);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (p_param->softap_max_conn_num.max_conn_num > 4) {
            return;
        }
        ap_config.ap.max_connection = p_param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP MAX CONN NUM %d", ap_config.ap.max_connection);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (p_param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
            return;
        }
        ap_config.ap.authmode = p_param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP AUTH MODE %d", ap_config.ap.authmode);
        break;

    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (p_param->softap_channel.channel > 13) {
            return;
        }
        ap_config.ap.channel = p_param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        BLUFI_INFO("Recv SOFTAP CHANNEL %d", ap_config.ap.channel);
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
        {
            wifi_scan_config_t scanConf = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            esp_err_t rcode = esp_wifi_scan_start(&scanConf,true);
            if (ESP_OK != rcode) {
                BLUFI_ERROR("WiFi scan_start error (0x%X) %s",rcode,esp_err_to_name(rcode));
            }
        }
        break;

    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        BLUFI_INFO("Recv Custom Data %d",p_param->custom_data.data_len);
        esp_log_buffer_hex("Custom Data",p_param->custom_data.data,p_param->custom_data.data_len);
        break;

    case ESP_BLUFI_EVENT_RECV_USERNAME:
    case ESP_BLUFI_EVENT_RECV_CA_CERT:
    case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
    case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
    case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
    case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        ESP_LOGW(BLUFI_TAG,"Ignoring event %d",event);
        break;

    default:
        BLUFI_ERROR("Unexpected event %d",event);
        break;
    }
}

//-----------------------------------------------------------------------------

static esp_blufi_callbacks_t callbacks = {
    .event_cb = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

//-----------------------------------------------------------------------------

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p_param __attribute__((unused)))
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        BLUFI_INFO("%s: ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT",__func__);
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

//-----------------------------------------------------------------------------

esp_err_t mlab_blufi_start(void)
{
    const char *p_errtag = "";
    esp_err_t ret = ESP_OK;

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ESP_OK == ret) {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ESP_OK != ret) {
            p_errtag = "bt controller init failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ESP_OK != ret) {
            p_errtag = "bt controller enable failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_bluedroid_init();
        if (ESP_OK != ret) {
            p_errtag = "bluedroid init failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_bluedroid_enable();
        if (ESP_OK == ret) {
            BLUFI_INFO("BD ADDR: "ESP_BD_ADDR_STR"",ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
            BLUFI_INFO("BLUFI VERSION %04X",esp_blufi_get_version());
        } else {
            p_errtag = "bluedroid enable failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_ble_gap_register_callback(gap_event_handler);
        if (ESP_OK != ret) {
            p_errtag = "gap register failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_blufi_register_callbacks(&callbacks);
        if (ESP_OK != ret) {
            p_errtag = "BluFi register failed";
        }
    }

    if (ESP_OK == ret) {
        ret = esp_blufi_profile_init();
        if (ESP_OK != ret) {
            p_errtag = "BluFi profile init failed";
        }
    }

    if (ESP_OK != ret) {
        BLUFI_ERROR("%s: %s: error (0x%X) %s",__func__,p_errtag,ret,esp_err_to_name(ret));
    }

    return ret;
}

//-----------------------------------------------------------------------------

# endif // CONFIG_MLAB_BLUFI
#endif // CONFIG_BT_ENABLED

//=============================================================================
// EOF bluefi.c
