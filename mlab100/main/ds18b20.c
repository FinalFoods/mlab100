// ds18b20.c
//=============================================================================
/* 
 * DS18B20 Temperature Sensor
 *
 * NOTE: this implementation assumes only one 1-Wire device connected to the bus.
 *
 * https://www.maximintegrated.com/en/app-notes/index.mvp/id/126
 *
*/

//=============================================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

//-----------------------------------------------------------------------------

// GPIO pin for DS18B20 temperature sensor
#define DS_GPIO (32)

//=============================================================================

//-----------------------------------------------------------------------------
// Send one bit to the 1-Wire bus

static void ds18b20_send(char bit){
  gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(DS_GPIO,0);
  ets_delay_us(5);
  if (bit == 1) 
    gpio_set_level(DS_GPIO,1);
  ets_delay_us(80);
  gpio_set_level(DS_GPIO,1);
}

//-----------------------------------------------------------------------------
// Read one bit from the 1-Wire bus

static unsigned char ds18b20_read(void){
  unsigned char presence = 0;

  gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(DS_GPIO, 0);
  ets_delay_us(2);
  gpio_set_level(DS_GPIO, 1);
  ets_delay_us(15);
  gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
  if (gpio_get_level(DS_GPIO) == 1) 
    presence = 1; 
  else 
    presence = 0;
  return(presence);
}

//-----------------------------------------------------------------------------
// Send one byte to the 1-Wire bus

static void ds18b20_send_byte(char data){
  unsigned char i;
  unsigned char x;

  for (i = 0; i < 8; i++) {
    x = data >> i;
    x &= 0x01;
    ds18b20_send(x);
  }
  ets_delay_us(100);
}

//-----------------------------------------------------------------------------
// Read one byte from the 1-Wire bus

static unsigned char ds18b20_read_byte(void){
  unsigned char i;
  unsigned char data = 0;

  for (i = 0; i < 8; i++) {
    if (ds18b20_read()) 
      data |= 0x01 << i;
    ets_delay_us(15);
  }
  return(data);
}

//-----------------------------------------------------------------------------
// Send a reset pulse to the 1-Wire bus

static unsigned char ds18b20_RST_PULSE(void){
  unsigned char presence;

  gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(DS_GPIO, 0);
  ets_delay_us(500);
  gpio_set_level(DS_GPIO, 1);
  gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
  ets_delay_us(30);
  if (gpio_get_level(DS_GPIO) == 0) 
    presence = 1; 
  else 
    presence = 0;
  ets_delay_us(470);
  if (gpio_get_level(DS_GPIO) == 1) 
    presence = 1; 
  else 
    presence = 0;
  return presence;
}

//-----------------------------------------------------------------------------
// Return a temperature reading from the DS18B20

float ds18b20_get_temp(void) {
  unsigned char check;
  char temp1 = 0, temp2 = 0;

  check = ds18b20_RST_PULSE();
  if (check == 1) {
    ds18b20_send_byte(0xCC);  // skip ROM
    ds18b20_send_byte(0x44);  // Convert T (move value into Scratchpad)
    vTaskDelay(750 / portTICK_RATE_MS);
    check = ds18b20_RST_PULSE();
    ds18b20_send_byte(0xCC);  // skip ROM
    ds18b20_send_byte(0xBE);  // Read Scratchpad
    temp1 = ds18b20_read_byte();
    temp2 = ds18b20_read_byte();
    check = ds18b20_RST_PULSE();
    //    printf("\ttemp1: 0x%X temp2: 0x%X ds18b20_RST_PULSE: %d\n", check, temp1, temp2); // DEBUG
    float temp = 0;
    temp = (float)(temp1 + (temp2 * 256)) / 16;
    return temp;
  }
  else
    return 0;
  }

//-----------------------------------------------------------------------------
// Initialize the DS18B20

void ds18b20_init(void){
  gpio_pad_select_gpio(DS_GPIO);
}

//=============================================================================
// EOF ds18b20.c
