// mlab.h
//==============================================================================

#if !defined(__mlab_h)
# define __mlab_h (1)

//-----------------------------------------------------------------------------

#include <stdio.h>
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

// TODO:DEFINE: any pre-processor manifests needed

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

//-----------------------------------------------------------------------------

// TODO: provide any exported prototypes, etc.

//-----------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif // __cplusplus

//-----------------------------------------------------------------------------

#endif // !__mlab_h

//==============================================================================
// EOF mlab.h
