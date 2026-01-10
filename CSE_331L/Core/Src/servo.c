#include "servo.h"

/* For typical 180° servo:
   0°   ~ 1000 us
   180° ~ 2000 us
   60°  ~ 1333 us
*/
static uint16_t angle_to_pulse_us(uint8_t angle)
{
    if (angle > 180) angle = 180;

    // linear map: 1000us + angle*(1000/180)
    // pulse = 1000 + (angle * 1000) / 180
    return (uint16_t)(1000U + ((uint32_t)angle * 1000U) / 180U);
}

void Servo_Init(Servo_HandleTypeDef *hs, TIM_HandleTypeDef *htim, uint32_t channel)
{
    hs->htim = htim;
    hs->channel = channel;
}

void Servo_SetAngle(Servo_HandleTypeDef *hs, uint8_t angle)
{
    uint16_t pulse_us = angle_to_pulse_us(angle);

    /* With timer at 1MHz: compare value == pulse width in us */
    __HAL_TIM_SET_COMPARE(hs->htim, hs->channel, pulse_us);
}
