/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : FINAL – DHT11 Alarm + OLED Menu + Keypad + Ultrasonic + Servo (PA8 TIM1_CH1) + Soil Moisture (ADC)
  *
  * NOTE (CubeMX for ADC/Soil):
  *   - PA0 = ADC1_IN0 (Analog)
  *   - ADC Prescaler MUST be /6 (ADC clk <= 14 MHz). Your clock screenshot showed /2 (36MHz) which is invalid.
  *
  * Soil sensor wiring (analog mode):
  *   VCC -> 3.3V (preferred)
  *   GND -> GND
  *   AO  -> PA0 (ADC1_IN0)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "dht11.h"
#include <stdio.h>
#include "soil_moisture.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define TEMP_THRESHOLD_C        30
#define DIST_THRESHOLD_CM       10

#define PAGE_MENU               0
#define PAGE_DHT11              1
#define PAGE_USONIC             2
#define PAGE_SOIL               3

#define MOISTURE_THRESHOLD_PCT  15

#define SERVO_ANGLE_CLOSED      0
#define SERVO_ANGLE_OPEN        90

#define SERVO_PULSE_MIN_US      1000   // ~0°
#define SERVO_PULSE_MAX_US      2000   // ~180°
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;   /* DHT11 timing */
TIM_HandleTypeDef htim3;   /* Ultrasonic timing (1us tick) */
TIM_HandleTypeDef htim1;   /* Servo PWM TIM1_CH1 on PA8 */
ADC_HandleTypeDef hadc1;   /* Soil moisture ADC */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);

/* USER CODE BEGIN 0 */
/* ================= GLOBAL SENSOR STATE ================= */
static int g_temperature = 0;
static int g_humidity = 0;
static uint8_t g_dht_ok = 0;
static uint8_t g_alarm_active = 0; /* overheat alarm */

/* ================= ULTRASONIC STATE ================= */
static int g_distance_cm = -1;
static uint8_t g_object_near = 0;

/* ================= SOIL MOISTURE STATE ================= */
static uint8_t g_soil_ok = 0;
static uint8_t g_moisture_pct = 0;
static uint8_t g_soil_dry = 0;          /* 1 when moisture below threshold */

/* --- ADD: soil buzzer mute (acknowledge) --- */
static uint8_t g_soil_buzzer_muted = 0; /* 1 = buzzer muted for soil-dry alarm */

/* ================= BUZZER (ACTIVE-HIGH) ================= */
#define BUZZER_ON()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define BUZZER_OFF()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)

/* ================= KEYPAD CONFIG ================= */
/* Rows: PB8–PB11 */
#define R1_PIN GPIO_PIN_8
#define R2_PIN GPIO_PIN_9
#define R3_PIN GPIO_PIN_10
#define R4_PIN GPIO_PIN_11

/* Cols: PA4–PA7 */
#define C1_PIN GPIO_PIN_4
#define C2_PIN GPIO_PIN_5
#define C3_PIN GPIO_PIN_6
#define C4_PIN GPIO_PIN_7

static const char keymap[4][4] =
{
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

static void Rows_AllHigh(void)
{
    HAL_GPIO_WritePin(GPIOB, R1_PIN | R2_PIN | R3_PIN | R4_PIN, GPIO_PIN_SET);
}

static void Row_Low(uint8_t r)
{
    Rows_AllHigh();
    if (r == 0) HAL_GPIO_WritePin(GPIOB, R1_PIN, GPIO_PIN_RESET);
    if (r == 1) HAL_GPIO_WritePin(GPIOB, R2_PIN, GPIO_PIN_RESET);
    if (r == 2) HAL_GPIO_WritePin(GPIOB, R3_PIN, GPIO_PIN_RESET);
    if (r == 3) HAL_GPIO_WritePin(GPIOB, R4_PIN, GPIO_PIN_RESET);
}

static void settle_short(void)
{
    for (volatile int i = 0; i < 200; i++) { __NOP(); }
}

/* Stable keypad scan (returns one key press) */
static char Keypad_GetKeyOnce(void)
{
    static char last = 0;
    static uint32_t lastTime = 0;

    for (uint8_t r = 0; r < 4; r++)
    {
        Row_Low(r);
        settle_short();

        char k = 0;
        if (HAL_GPIO_ReadPin(GPIOA, C1_PIN) == GPIO_PIN_RESET) k = keymap[r][0];
        if (HAL_GPIO_ReadPin(GPIOA, C2_PIN) == GPIO_PIN_RESET) k = keymap[r][1];
        if (HAL_GPIO_ReadPin(GPIOA, C3_PIN) == GPIO_PIN_RESET) k = keymap[r][2];
        if (HAL_GPIO_ReadPin(GPIOA, C4_PIN) == GPIO_PIN_RESET) k = keymap[r][3];

        if (k && last == 0 && (HAL_GetTick() - lastTime > 25))
        {
            last = k;
            lastTime = HAL_GetTick();
            return k;
        }
        if (!k) last = 0;
    }
    return 0;
}

/* ================= ULTRASONIC (HC-SR04) ================= */
/* Your wiring: TRIG=PB4, ECHO=PB5 */
#define US_TRIG_PORT GPIOB
#define US_TRIG_PIN  GPIO_PIN_4
#define US_ECHO_PORT GPIOB
#define US_ECHO_PIN  GPIO_PIN_5

static void delay_us_tim3(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < us) { }
}

/* Returns cm >=0, or negative error */
static int Ultrasonic_ReadDistanceCm(void)
{
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);
    delay_us_tim3(2);

    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_SET);
    delay_us_tim3(10);
    HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);

    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (HAL_GPIO_ReadPin(US_ECHO_PORT, US_ECHO_PIN) == GPIO_PIN_RESET)
    {
        if (__HAL_TIM_GET_COUNTER(&htim3) > 30000) return -1;
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (HAL_GPIO_ReadPin(US_ECHO_PORT, US_ECHO_PIN) == GPIO_PIN_SET)
    {
        if (__HAL_TIM_GET_COUNTER(&htim3) > 30000) return -2;
    }

    uint32_t echo_us = __HAL_TIM_GET_COUNTER(&htim3);
    int cm = (int)(echo_us / 58U);
    if (cm < 2 || cm > 400) return -3;
    return cm;
}

/* ================= SERVO (TIM1_CH1 on PA8) ================= */
static uint16_t Servo_AngleToPulseUs(uint8_t angle)
{
    if (angle > 180) angle = 180;
    return (uint16_t)(SERVO_PULSE_MIN_US +
        ((uint32_t)angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / 180U);
}

static void Servo_SetAngle(uint8_t angle)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, Servo_AngleToPulseUs(angle));
}

/* ================= OLED PAGES ================= */
static void OLED_Menu(void)
{
    SSD1306_Fill(Black);
    SSD1306_SetCursor(0,0);
    SSD1306_WriteString(" MENU");
    SSD1306_SetCursor(0,16);
    SSD1306_WriteString(" 1:DHT11 Sensor");
    SSD1306_SetCursor(0,32);
    SSD1306_WriteString(" 2:Ultrasonic Sensor");
    SSD1306_SetCursor(0,48);
    SSD1306_WriteString(" 3:Soil Moisture Sensor");
    SSD1306_UpdateScreen();
}

static void OLED_DHT11(void)
{
    SSD1306_Fill(Black);
    SSD1306_SetCursor(0,0);
    SSD1306_WriteString(" DHT11 SENSOR");

    if (g_dht_ok)
    {
        char l1[20], l2[20];
        sprintf(l1, " Temp: %d C", g_temperature);
        sprintf(l2, " Hum : %d %%", g_humidity);

        SSD1306_SetCursor(0,16);
        SSD1306_WriteString(l1);
        SSD1306_SetCursor(0,32);
        SSD1306_WriteString(l2);

        SSD1306_SetCursor(0,48);
        if (g_alarm_active)
            SSD1306_WriteString(" !!! OVERHEAT !!!");
        else
            SSD1306_WriteString(" Status: OK (Menu:0)");
    }
    else
    {
        SSD1306_SetCursor(0,16);
        SSD1306_WriteString(" DHT ERROR");
        SSD1306_SetCursor(0,48);
        SSD1306_WriteString(" (Menu:0)");
    }

    SSD1306_UpdateScreen();
}

static void OLED_Ultrasonic(void)
{
    SSD1306_Fill(Black);
    SSD1306_SetCursor(0,0);
    SSD1306_WriteString(" Ultrasonic Sensor");

    SSD1306_SetCursor(0,16);
    if (g_distance_cm >= 0)
    {
        char l1[20];
        sprintf(l1, " Dist: %d cm", g_distance_cm);
        SSD1306_WriteString(l1);
    }
    else
    {
        SSD1306_WriteString(" Dist: --");
    }

    SSD1306_SetCursor(0,32);
    if (g_distance_cm >= 0)
    {
        if (g_distance_cm <= DIST_THRESHOLD_CM)
            SSD1306_WriteString(" Door: OPENING");
        else
            SSD1306_WriteString(" Door: CLOSED");
    }
    else
    {
        SSD1306_WriteString(" Door: --");
    }

    SSD1306_SetCursor(0,48);
    SSD1306_WriteString(" Near:");
    SSD1306_WriteString(g_object_near ? "YES" : "NO");
    SSD1306_WriteString(" (Menu:0)");

    SSD1306_UpdateScreen();
}

/* --- Soil moisture OLED page (with A to mute) --- */
static void OLED_Soil(void)
{
    SSD1306_Fill(Black);
    SSD1306_SetCursor(0,0);
    SSD1306_WriteString(" Soil Moisture Sensor");

    SSD1306_SetCursor(0,16);
    if (g_soil_ok)
    {
        char l1[20];
        sprintf(l1, " Moist: %u %%", g_moisture_pct);
        SSD1306_WriteString(l1);

        SSD1306_SetCursor(0,32);
        if (g_soil_dry)
        {
            if (g_soil_buzzer_muted)
                SSD1306_WriteString(" Status: DRY (MUTED)");
            else
                SSD1306_WriteString(" Status: DRY !!!");
        }
        else
        {
            SSD1306_WriteString(" Status: OK");
        }
    }
    else
    {
        SSD1306_WriteString(" Sensor: ERROR");
    }

    SSD1306_SetCursor(0,48);
    if (g_soil_dry && !g_soil_buzzer_muted)
			SSD1306_WriteString(" Mute buzzer:A(Menu:0)");
    else
        SSD1306_WriteString(" 0:Menu");

    SSD1306_UpdateScreen();
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();

  /* Start timers */
  HAL_TIM_Base_Start(&htim3);                 /* ultrasonic microsecond timer */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   /* servo PWM on PA8 */
  __HAL_TIM_MOE_ENABLE(&htim1);               /* ensure TIM1 outputs are enabled */

  /* USER CODE BEGIN 2 */
  SSD1306_Init(&hi2c1);
  DHT11_Init(GPIOA, GPIO_PIN_1, &htim2);

  /* Soil moisture driver init (temporary calibration) */
  SoilMoisture_Cal cal;
  cal.dry_raw = 3500;  /* adjust later by observing raw ADC in dry soil */
  cal.wet_raw = 1500;  /* adjust later by observing raw ADC in water/wet soil */
  SoilMoisture_Init(&hadc1, cal);

  Rows_AllHigh();
  BUZZER_OFF();
  HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);

  Servo_SetAngle(SERVO_ANGLE_CLOSED);

  uint8_t page = PAGE_MENU;
  uint32_t lastDhtRead  = 0;
  uint32_t lastUsRead   = 0;
  uint32_t lastSoilRead = 0;

  uint8_t lastDoorOpening = 255;
  /* USER CODE END 2 */

  while (1)
  {
    /* ===== DHT11 + OVERHEAT ALARM ===== */
    if (HAL_GetTick() - lastDhtRead >= 1000)
    {
        lastDhtRead = HAL_GetTick();
        DHT11_Data d = DHT11_Read();

        if (d.ok && d.temperature >= 0 && d.temperature <= 60 && d.humidity <= 100)
        {
            g_temperature = d.temperature;
            g_humidity    = d.humidity;
            g_dht_ok      = 1;

            if (g_temperature >= TEMP_THRESHOLD_C)
            {
                g_alarm_active = 1;
                BUZZER_ON();
            }
            else
            {
                g_alarm_active = 0;
                /* Do not force buzzer OFF here because soil logic may need it.
                   Soil section will apply combined logic periodically. */
            }
        }
        else
        {
            g_dht_ok = 0;
            g_alarm_active = 0;
            /* Same note: soil section will decide buzzer state. */
        }
    }

    /* ===== ULTRASONIC ===== */
    if (HAL_GetTick() - lastUsRead >= 250)
    {
        lastUsRead = HAL_GetTick();
        int cm = Ultrasonic_ReadDistanceCm();

        if (cm >= 0)
        {
            g_distance_cm = cm;
            g_object_near = (g_distance_cm <= DIST_THRESHOLD_CM) ? 1 : 0;
        }
        else
        {
            g_distance_cm = -1;
            g_object_near = 0;
        }
    }

    /* ===== SOIL MOISTURE (ADC) ===== */
    if (HAL_GetTick() - lastSoilRead >= 700)
    {
        lastSoilRead = HAL_GetTick();
        SoilMoisture_Data sm = SoilMoisture_Read();

        if (sm.ok)
        {
            g_soil_ok = 1;
            g_moisture_pct = sm.percent;
            g_soil_dry = (g_moisture_pct < MOISTURE_THRESHOLD_PCT) ? 1 : 0;
        }
        else
        {
            g_soil_ok = 0;
            g_soil_dry = 0;
        }

        /* Auto clear mute when soil is no longer dry */
        if (g_soil_ok && (g_soil_dry == 0))
        {
            g_soil_buzzer_muted = 0;
        }

        /* Combined buzzer logic:
           - Overheat always can ring
           - Soil dry rings only if NOT muted
        */
        if (g_alarm_active || (g_soil_dry && !g_soil_buzzer_muted))
            BUZZER_ON();
        else
            BUZZER_OFF();
    }

    /* ===== SERVO ACTION ===== */
    {
        uint8_t doorOpening = (g_distance_cm >= 0 && g_distance_cm <= DIST_THRESHOLD_CM) ? 1 : 0;

        if (doorOpening != lastDoorOpening)
        {
            lastDoorOpening = doorOpening;
            Servo_SetAngle(doorOpening ? SERVO_ANGLE_OPEN : SERVO_ANGLE_CLOSED);
        }
    }

    /* ===== KEYPAD ===== */
    char key = Keypad_GetKeyOnce();

    /* Soil page: press A to mute soil buzzer (only if not overheating) */
    if (!g_alarm_active && page == PAGE_SOIL && key == 'A')
    {
        g_soil_buzzer_muted = 1;

        /* Stop buzzer immediately if it was ringing due to soil dryness only */
        if (!g_alarm_active) BUZZER_OFF();
    }

    /* Page navigation:
       - Only block navigation during OVERHEAT (g_alarm_active)
       - Soil-dry does NOT block navigation (so you can go to soil page and mute)
    */
    if (!g_alarm_active)
    {
        if (key == '1') page = PAGE_DHT11;
        if (key == '2') page = PAGE_USONIC;
        if (key == '3') page = PAGE_SOIL;
        if (key == '0') page = PAGE_MENU;
    }
    else
    {
        page = PAGE_DHT11;
    }

    /* ===== OLED ===== */
    if (page == PAGE_MENU) OLED_Menu();
    else if (page == PAGE_DHT11) OLED_DHT11();
    else if (page == PAGE_USONIC) OLED_Ultrasonic();
    else OLED_Soil();

    HAL_Delay(120);
  }
}

/* ===== INIT FUNCTIONS ===== */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  HAL_I2C_Init(&hi2c1);
}

static void MX_TIM2_Init(void)
{
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFF;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_Base_Init(&htim2);
}

static void MX_TIM3_Init(void)
{
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71; /* 1us tick */
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 0xFFFF;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_Base_Init(&htim3);
}

/* IMPORTANT: CubeMX should generate this with Period=19999 and Pulse=1000 */
static void MX_TIM1_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 19999; /* 50Hz PWM */
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  HAL_TIM_PWM_Init(&htim1);

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = SERVO_PULSE_MIN_US; /* 1000us = ~0° */
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);

  HAL_TIM_MspPostInit(&htim1);
}

/* ADC1 init for soil moisture on PA0 (ADC1_IN0) */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  __HAL_RCC_ADC1_CLK_ENABLE();

  /* Ensure ADC prescaler is DIV6 if available in your HAL */
#ifdef RCC_ADCPCLK2_DIV6
  __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
#endif

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  HAL_ADC_Init(&hadc1);

  sConfig.Channel = ADC_CHANNEL_0; /* PA0 */
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);

  HAL_ADCEx_Calibration_Start(&hadc1);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* Free PB4 (NJTRST) if you use PB4 for Ultrasonic TRIG */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();

  /* Set initial output levels */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | R1_PIN | R2_PIN | R3_PIN | R4_PIN | US_TRIG_PIN, GPIO_PIN_RESET);

  /* PB outputs: buzzer + keypad rows + ultrasonic TRIG */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | R1_PIN | R2_PIN | R3_PIN | R4_PIN | US_TRIG_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PA inputs: DHT11 + keypad columns */
  GPIO_InitStruct.Pin = GPIO_PIN_1 | C1_PIN | C2_PIN | C3_PIN | C4_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB input: ultrasonic ECHO */
  GPIO_InitStruct.Pin = US_ECHO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PA0 is Analog (ADC1_IN0) via CubeMX, no GPIO init needed here */

  BUZZER_OFF();
  Rows_AllHigh();
  HAL_GPIO_WritePin(US_TRIG_PORT, US_TRIG_PIN, GPIO_PIN_RESET);
}

/* USER CODE BEGIN 4 */
/* If your project needs Error_Handler, keep this. */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
/* USER CODE END 4 */
