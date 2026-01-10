#ifndef __SERVO_H__
#define __SERVO_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} Servo_HandleTypeDef;

/* Timer must run at 1MHz and PWM period must be 20000 ticks (20ms) */
void Servo_Init(Servo_HandleTypeDef *hs, TIM_HandleTypeDef *htim, uint32_t channel);

/* angle: 0..180 */
void Servo_SetAngle(Servo_HandleTypeDef *hs, uint8_t angle);

#endif
