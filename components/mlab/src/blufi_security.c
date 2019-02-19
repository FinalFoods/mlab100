// blufi_security.c
//=============================================================================
/* Copied from esp-idf/examples/bluetooth/blufi/main/blufi_security.c and
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

#include "mlab_blufi.h"

//-----------------------------------------------------------------------------

#if CONFIG_BT_ENABLED

//-----------------------------------------------------------------------------

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "rom/crc.h"

//-----------------------------------------------------------------------------
/* The SEC_TYPE_xxx is for the self-defined packet data type in the procedure of
  "BLUFI negotiate key". If the user implements another negotiation procedure to
  exchange (or generate) the key, then these should be redefined as
  appropriate. */

#define SEC_TYPE_DH_PARAM_LEN   (0x00)
#define SEC_TYPE_DH_PARAM_DATA  (0x01)
#define SEC_TYPE_DH_P           (0x02)
#define SEC_TYPE_DH_G           (0x03)
#define SEC_TYPE_DH_PUBLIC      (0x04)

//-----------------------------------------------------------------------------

#define DH_SELF_PUB_KEY_LEN     (128)
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)

#define SHARE_KEY_LEN           (128)
#define SHARE_KEY_BIT_LEN       (SHARE_KEY_LEN * 8)

#define PSK_LEN                 (16)

struct blufi_security {
    uint8_t self_public_key[DH_SELF_PUB_KEY_LEN];
    uint8_t share_key[SHARE_KEY_LEN];
    size_t share_len;
    uint8_t psk[PSK_LEN];
    uint8_t *p_dh_param;
    int dh_param_len;
    uint8_t iv[16];
    mbedtls_dhm_context dhm;
    mbedtls_aes_context aes;
};

static struct blufi_security *p_blufi_sec;

//-----------------------------------------------------------------------------

static int myrand(void *p_rng_state __attribute__((unused)),unsigned char *p_output,size_t len)
{
    esp_fill_random(p_output,len);
    return ESP_OK;
}

//-----------------------------------------------------------------------------

extern void btc_blufi_report_error(esp_blufi_error_state_t state);

void blufi_dh_negotiate_data_handler(uint8_t *p_data,int len,uint8_t **pp_output_data,int *p_output_len,bool *p_need_free)
{
    int ret;
    uint8_t type = p_data[0];

    if (NULL == p_blufi_sec) {
        BLUFI_ERROR("BLUFI Security is not initialized");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    switch (type) {
    case SEC_TYPE_DH_PARAM_LEN:
        p_blufi_sec->dh_param_len = ((p_data[1] << 8) | p_data[2]);
        if (p_blufi_sec->p_dh_param) {
            free(p_blufi_sec->p_dh_param);
            p_blufi_sec->p_dh_param = NULL;
        }
        p_blufi_sec->p_dh_param = (uint8_t *)malloc(p_blufi_sec->dh_param_len);
        if (NULL == p_blufi_sec->p_dh_param) {
            btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
            BLUFI_ERROR("%s, malloc failed",__func__);
            return;
        }
        break;

    case SEC_TYPE_DH_PARAM_DATA:
        {
            if (NULL == p_blufi_sec->p_dh_param) {
                BLUFI_ERROR("%s, p_blufi_sec->p_dh_param == NULL",__func__);
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }

            uint8_t *param = p_blufi_sec->p_dh_param;
            memcpy(p_blufi_sec->p_dh_param, &p_data[1],p_blufi_sec->dh_param_len);
            ret = mbedtls_dhm_read_params(&p_blufi_sec->dhm, &param, &param[p_blufi_sec->dh_param_len]);
            if (ret) {
                BLUFI_ERROR("%s read param failed %d", __func__, ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }
            free(p_blufi_sec->p_dh_param);
            p_blufi_sec->p_dh_param = NULL;
            ret = mbedtls_dhm_make_public(&p_blufi_sec->dhm,(int)mbedtls_mpi_size(&p_blufi_sec->dhm.P),p_blufi_sec->self_public_key,p_blufi_sec->dhm.len,myrand,NULL);
            if (ret) {
                BLUFI_ERROR("%s make public failed %d", __func__, ret);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            mbedtls_dhm_calc_secret( &p_blufi_sec->dhm,
                                     p_blufi_sec->share_key,
                                     SHARE_KEY_BIT_LEN,
                                     &p_blufi_sec->share_len,
                                     NULL, NULL);

            mbedtls_md5(p_blufi_sec->share_key, p_blufi_sec->share_len, p_blufi_sec->psk);

            mbedtls_aes_setkey_enc(&p_blufi_sec->aes, p_blufi_sec->psk, 128);
        
            /* alloc output data */
            if (pp_output_data) {
                *pp_output_data = &p_blufi_sec->self_public_key[0];
            }
            if (p_output_len) {
                *p_output_len = p_blufi_sec->dhm.len;
            }
            if (p_need_free) {
                *p_need_free = false;
            }
        }
        break;

    case SEC_TYPE_DH_P:
        break;

    case SEC_TYPE_DH_G:
        break;

    case SEC_TYPE_DH_PUBLIC:
        break;
    }

    return;
}

//-----------------------------------------------------------------------------

int blufi_aes_encrypt(uint8_t iv8,uint8_t *p_crypt_data,int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0,p_blufi_sec->iv,sizeof(p_blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&p_blufi_sec->aes,MBEDTLS_AES_ENCRYPT,crypt_len,&iv_offset,iv0,p_crypt_data,p_crypt_data);
    if (ret) {
        return -1;
    }

    return crypt_len;
}

//-----------------------------------------------------------------------------

int blufi_aes_decrypt(uint8_t iv8,uint8_t *p_crypt_data,int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0,p_blufi_sec->iv,sizeof(p_blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&p_blufi_sec->aes,MBEDTLS_AES_DECRYPT,crypt_len,&iv_offset,iv0,p_crypt_data,p_crypt_data);
    if (ret) {
        return -1;
    }

    return crypt_len;
}

//-----------------------------------------------------------------------------

uint16_t blufi_crc_checksum(uint8_t iv8 __attribute__((unused)),uint8_t *p_data,int len)
{
    return crc16_be(0x00000000,(uint8_t const *)p_data,(uint32_t)len); // ESP32 ROM routine
}

//-----------------------------------------------------------------------------

esp_err_t blufi_security_init(void)
{
    esp_err_t rcode = ESP_FAIL;

    p_blufi_sec = (struct blufi_security *)malloc(sizeof(struct blufi_security));
    if (p_blufi_sec) {
        memset(p_blufi_sec,'\0',sizeof(struct blufi_security));

        mbedtls_dhm_init(&p_blufi_sec->dhm);
        mbedtls_aes_init(&p_blufi_sec->aes);

        memset(p_blufi_sec->iv,'\0',sizeof(p_blufi_sec->iv));
        rcode = ESP_OK;
    }

    return rcode;
}

//-----------------------------------------------------------------------------

void blufi_security_deinit(void)
{
    if (p_blufi_sec) {
        if (p_blufi_sec->p_dh_param){
            free(p_blufi_sec->p_dh_param);
            p_blufi_sec->p_dh_param = NULL;
        }

        mbedtls_dhm_free(&p_blufi_sec->dhm);
        mbedtls_aes_free(&p_blufi_sec->aes);

        memset(p_blufi_sec,'\0',sizeof(struct blufi_security));
        free(p_blufi_sec);
        p_blufi_sec =  NULL;
    }
}

//-----------------------------------------------------------------------------

#endif // CONFIG_BT_ENABLED

//=============================================================================
// EOF blufi_security.c
