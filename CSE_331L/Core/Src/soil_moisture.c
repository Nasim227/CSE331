#include "soil_moisture.h"

static ADC_HandleTypeDef *g_hadc = NULL;
static SoilMoisture_Cal g_cal;

static uint8_t map_to_percent(uint16_t raw, uint16_t dry_raw, uint16_t wet_raw)
{
    if (wet_raw == dry_raw) return 0;

    if (wet_raw > dry_raw)
    {
        if (raw <= dry_raw) return 0;
        if (raw >= wet_raw) return 100;
        return (uint8_t)(((uint32_t)(raw - dry_raw) * 100U) / (wet_raw - dry_raw));
    }
    else
    {
        if (raw >= dry_raw) return 0;
        if (raw <= wet_raw) return 100;
        return (uint8_t)(((uint32_t)(dry_raw - raw) * 100U) / (dry_raw - wet_raw));
    }
}

void SoilMoisture_Init(ADC_HandleTypeDef *hadc, SoilMoisture_Cal cal)
{
    g_hadc = hadc;
    g_cal = cal;
}

SoilMoisture_Data SoilMoisture_Read(void)
{
    SoilMoisture_Data out = {0};
    if (g_hadc == NULL)
    {
        out.ok = 0;
        return out;
    }

    if (HAL_ADC_Start(g_hadc) != HAL_OK)
    {
        out.ok = 0;
        return out;
    }

    if (HAL_ADC_PollForConversion(g_hadc, 20) != HAL_OK)
    {
        HAL_ADC_Stop(g_hadc);
        out.ok = 0;
        return out;
    }

    out.raw = (uint16_t)HAL_ADC_GetValue(g_hadc);
    HAL_ADC_Stop(g_hadc);

    out.percent = map_to_percent(out.raw, g_cal.dry_raw, g_cal.wet_raw);
    out.ok = 1;
    return out;
}
