#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct
{
    uint16_t raw;     // ADC raw (0..4095)
    uint8_t  percent; // 0..100
    uint8_t  ok;      // 1=valid
} SoilMoisture_Data;

typedef struct
{
    uint16_t dry_raw;
    uint16_t wet_raw;
} SoilMoisture_Cal;

void SoilMoisture_Init(ADC_HandleTypeDef *hadc, SoilMoisture_Cal cal);
SoilMoisture_Data SoilMoisture_Read(void);

#endif
