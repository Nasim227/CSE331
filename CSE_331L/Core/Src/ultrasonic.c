#include "ultrasonic.h"

/* ---------- Internal microsecond delay using 1MHz timer ---------- */
static void delay_us(TIM_HandleTypeDef *htim, uint16_t us)
{
    __HAL_TIM_SET_COUNTER(htim, 0);
    while (__HAL_TIM_GET_COUNTER(htim) < us) { }
}

/**
 * We assume typical HC-SR04 behavior:
 * - TRIG high for 10us
 * - ECHO pulse width in us is proportional to distance
 * - distance (cm) ˜ echo_us / 58
 */
void Ultrasonic_Init(Ultrasonic_HandleTypeDef *hus,
                     GPIO_TypeDef *trigPort, uint16_t trigPin,
                     GPIO_TypeDef *echoPort, uint16_t echoPin,
                     TIM_HandleTypeDef *htim_us)
{
    hus->trigPort = trigPort;
    hus->trigPin  = trigPin;
    hus->echoPort = echoPort;
    hus->echoPin  = echoPin;
    hus->htim_us  = htim_us;

    // Ensure TRIG starts low
    HAL_GPIO_WritePin(hus->trigPort, hus->trigPin, GPIO_PIN_RESET);
}

int Ultrasonic_ReadDistanceCm(Ultrasonic_HandleTypeDef *hus)
{
    // 1) Send TRIG pulse (10us)
    HAL_GPIO_WritePin(hus->trigPort, hus->trigPin, GPIO_PIN_RESET);
    delay_us(hus->htim_us, 2);

    HAL_GPIO_WritePin(hus->trigPort, hus->trigPin, GPIO_PIN_SET);
    delay_us(hus->htim_us, 10);
    HAL_GPIO_WritePin(hus->trigPort, hus->trigPin, GPIO_PIN_RESET);

    // 2) Wait for ECHO to go HIGH (start)
    __HAL_TIM_SET_COUNTER(hus->htim_us, 0);
    while (HAL_GPIO_ReadPin(hus->echoPort, hus->echoPin) == GPIO_PIN_RESET)
    {
        if (__HAL_TIM_GET_COUNTER(hus->htim_us) > 30000) // 30ms timeout
            return -1;
    }

    // 3) Measure how long ECHO stays HIGH
    __HAL_TIM_SET_COUNTER(hus->htim_us, 0);
    while (HAL_GPIO_ReadPin(hus->echoPort, hus->echoPin) == GPIO_PIN_SET)
    {
        if (__HAL_TIM_GET_COUNTER(hus->htim_us) > 30000) // 30ms timeout
            return -2;
    }

    uint32_t echo_us = __HAL_TIM_GET_COUNTER(hus->htim_us);

    // Convert to cm: HC-SR04 approx: cm = echo_us / 58
    int distance_cm = (int)(echo_us / 58U);

    // Basic sanity clamp
    if (distance_cm < 2 || distance_cm > 400) return -3;

    return distance_cm;
}
