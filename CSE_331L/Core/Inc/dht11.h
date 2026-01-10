#ifndef __DHT11_H__
#define __DHT11_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct {
    int temperature;
    int humidity;
    uint8_t ok;     // 1 = success, 0 = fail
} DHT11_Data;

void DHT11_Init(GPIO_TypeDef* port, uint16_t pin, TIM_HandleTypeDef* htim);
DHT11_Data DHT11_Read(void);

#endif
