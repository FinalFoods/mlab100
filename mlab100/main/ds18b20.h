// ds18b20.h
//=============================================================================

#if !defined(_ds18b20_)
#define __adc122s021_h (1)

/*
 * NOTE: this implementation assumes only one 1-Wire device connected to the bus.
 *
 * https://www.maximintegrated.com/en/app-notes/index.mvp/id/126
 */

//-----------------------------------------------------------------------------
/**
 * Initialise the 1-Wire bus.
 *
 * @param pin GPIO pin for the DS18D20 data line.
 */
extern void ds18b20_init(void);

/**
 * Get a temperature reading from the DS18D20. 
 *
 * @return temperature in degrees Celsius.
 */
extern float ds18b20_get_temp(void);

//-----------------------------------------------------------------------------

#endif // !_ds18b20_

//=============================================================================
// EOF _ds18b20_.h
