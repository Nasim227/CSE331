#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct
{
    GPIO_TypeDef *trigPort;
    uint16_t      trigPin;

    GPIO_TypeDef *echoPort;
    uint16_t      echoPin;

    TIM_HandleTypeDef *htim_us;   // timer running at 1 MHz (1 tick = 1 us)
} Ultrasonic_HandleTypeDef;

/**
 * Initialize ultrasonic handle.
 * Timer must already be initialized and started (HAL_TIM_Base_Start).
 */
void Ultrasonic_Init(Ultrasonic_HandleTypeDef *hus,
                     GPIO_TypeDef *trigPort, uint16_t trigPin,
                     GPIO_TypeDef *echoPort, uint16_t echoPin,
                     TIM_HandleTypeDef *htim_us);

/**
 * Read distance in cm.
 * Returns:
 *   >= 0 : distance (cm)
 *   <  0 : error/timeout
 */
int Ultrasonic_ReadDistanceCm(Ultrasonic_HandleTypeDef *hus);

#endif
