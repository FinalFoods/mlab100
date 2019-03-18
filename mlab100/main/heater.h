// heather.h
//=============================================================================

#if !defined(_heater_)
#define __heater_ (1)

#define LEDC_TEST_CH_NUM       (1)
#define LEDC_MAX_DUTY          (8192)
#define LEDC_TEST_DUTY         (0)
#define LEDC_TEST_ZERO          (0)

//-----------------------------------------------------------------------------
/**
 * Initialise the Heater device control.
 *
 */
extern void heater_init(void);

/**
 * Set Heater level.
 *
 */
extern int heater_set(int level);

//-----------------------------------------------------------------------------

#endif // !_heater_

//=============================================================================
// EOF heater.h