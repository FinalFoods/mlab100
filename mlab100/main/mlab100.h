/*
 * MLAB100 device application software.
 *
 * Copyright (C) 2019, Microbiota Labs Inc. and the MLAB100 contributors.
 *
 */
//=============================================================================

#if !defined(__mlab100_h)
# define __mlab100_h (1)

//-----------------------------------------------------------------------------

#define MLAB_MODEL	"100"

//-----------------------------------------------------------------------------
// Useful (standard) helper macros:

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

// DS18B20 temperature sensor connected to TEMP1W on J3
#define DS_GPIO (15)
// #define DS_GPIO (32)

// 1wire bus
#define ONEWIRE_PIN (15)

// control 3.3V regulator
#define CONTROL_3V3   (14)

// board status LEDs
#define GREEN_LED   (25)
#define YELLOW_LED	(26)
#define RED_LED		(27)

//  u/v blue leds
#define UV1_LED	(12)
#define UV2_LED	(13)

// heater PWM pin
#define HEATER_CTRL		(32)

// fan GPIP
#define FAN_CTRL	(17)

//-----------------------------------------------------------------------------

// Device current state
typedef enum {
    IDLE,
    COOLING,
    HEATING
}   heater_states_t;

// Device heating control
typedef struct  {
	heater_states_t state;
	float	temperature;
	float	setpoint;
	float	histeresis;
	float	adjustment;
} heater_t;

// string representation of state
#define STATE2STR(state) (state == IDLE ? "IDLE" : (state == HEATING ? "HEATING" : "COOLING"))

#define HEATER_OFF() {printf("heater_off()\n");heater_set(0);gpio_set_level(FAN_CTRL, 1);ds18b20_get_temp(sensors[0]);ds18b20_get_temp(sensors[1]);ds18b20_get_temp(sensors[2]);}
#define HEATER_ON() {printf("heater_on()\n");heater_set(1);gpio_set_level(FAN_CTRL, 0);ds18b20_get_temp(sensors[0]);ds18b20_get_temp(sensors[1]);ds18b20_get_temp(sensors[2]);}

#endif // !__mlab100_h

//=============================================================================
// EOF mlab100.h
