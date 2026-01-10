#include "dht11.h"

static GPIO_TypeDef* DHT_PORT;
static uint16_t DHT_PIN;
static TIM_HandleTypeDef* DHT_TIM;

static void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(DHT_TIM, 0);
    HAL_TIM_Base_Start(DHT_TIM);
    while (__HAL_TIM_GET_COUNTER(DHT_TIM) < us);
    HAL_TIM_Base_Stop(DHT_TIM);
}

static void SetPinOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);
}

static void SetPinInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // external pull-up required
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);
}

void DHT11_Init(GPIO_TypeDef* port, uint16_t pin, TIM_HandleTypeDef* htim)
{
    DHT_PORT = port;
    DHT_PIN = pin;
    DHT_TIM = htim;
}

static uint8_t ReadByte(void)
{
    uint8_t i, byte = 0;

    for (i = 0; i < 8; i++)
    {
        // wait for line to go HIGH
        uint32_t t = 0;
        while (!HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
        {
            if (++t > 3000) return 0;
        }

        // if still high after ~40us, it's '1'
        delay_us(40);
        if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
        {
            byte |= (1 << (7 - i));
        }

        // wait for line to go LOW
        t = 0;
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
        {
            if (++t > 3000) break;
        }
    }
    return byte;
}

DHT11_Data DHT11_Read(void)
{
    DHT11_Data out = {0};
    uint8_t rh_int, rh_dec, t_int, t_dec, checksum;

    // Start signal
    SetPinOutput();
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    HAL_Delay(18); // >=18ms
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    delay_us(30);  // 20-40us
    SetPinInput();

    // Response: LOW 80us, HIGH 80us
    uint32_t timeout = 0;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
    {
        if (++timeout > 5000) { out.ok = 0; return out; }
    }
    timeout = 0;
    while (!HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
    {
        if (++timeout > 5000) { out.ok = 0; return out; }
    }
    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN))
    {
        if (++timeout > 5000) { out.ok = 0; return out; }
    }

    // Read 5 bytes
    rh_int   = ReadByte();
    rh_dec   = ReadByte();
    t_int    = ReadByte();
    t_dec    = ReadByte();
    checksum = ReadByte();

    if ((uint8_t)(rh_int + rh_dec + t_int + t_dec) != checksum)
    {
        out.ok = 0;
        return out;
    }

    out.humidity = rh_int;
    out.temperature = t_int;
    out.ok = 1;
    return out;
}
