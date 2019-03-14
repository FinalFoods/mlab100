// heater.c
//=============================================================================
/* 
 * Heater Control -- using ESP32 integrated PWM LED Controller
 *
 *
*/

//=============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "mlab100.h"

// control the heater using LEDC channel 0
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (32)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0

#define LEDC_TEST_CH_NUM       (1)
#define LEDC_MAX_DUTY          (8192)
#define LEDC_TEST_DUTY         (40)
#define LEDC_TEST_ZERO          (0)
/*
 * Prepare and set configuration of timers 7232
 * that will be used by LED Controller
 *
 * With PWM frequency at 5 kHz, the maximum duty resolution is 13 bits. 
 * The duty may be set from 0 to 100% with resolution of ~0.012% 
 * (13 ** 2 = 8192 discrete levels of heater intensity).

 */
ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HS_MODE,           // high-speed timer mode
    .timer_num = LEDC_HS_TIMER            // high-speed timer index
};

//-----------------------------------------------------------------------------
// Initialize the heater
//
void heater_init(void){
    int ch;

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
        {
            .channel    = LEDC_HS_CH0_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_HS_CH0_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_HS_TIMER
        },
    };

    // Set LED Controller with different channels configurations
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
        ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, LEDC_TEST_DUTY);
        ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
    }
}

// Set the heater level. Level must be between 0 and LEDC_MAX_DUTY (8192)
//
int heater_set(int level) {
    if ((level < 0) || (level > LEDC_MAX_DUTY))
        return -1;

    ledc_set_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL, LEDC_TEST_DUTY);
    ledc_update_duty(LEDC_HS_MODE, LEDC_HS_CH0_CHANNEL);
    return 0;
}

//=============================================================================
// EOF heater.c
