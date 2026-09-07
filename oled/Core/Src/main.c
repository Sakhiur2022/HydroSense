/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Soil moisture + ML urgency
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fonts.h"
#include "ssd1306.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    URGENCY_NOT_URGENT = 0,
    URGENCY_SOON = 1,
    URGENCY_URGENT = 2
} UrgencyClass;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* AHT20 I2C address */
#define AHT20_ADDR (0x38 << 1)

/* Dummy values for unavailable sensors */
#define DUMMY_TEMPERATURE 25.0f
#define DUMMY_HUMIDITY    50.0f
#define DUMMY_LIGHT       500.0f
#define DUMMY_TREND       0.0f
#define DUMMY_HOUR        12.0f

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */

uint16_t readValue;

float temperature_C;
float humidity_pct;
float soil_moisture_pct;

float moisture_trend;
float light_lux;
float hour_of_day;

UrgencyClass urgency;

uint8_t aht20_ok;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);

/* USER CODE BEGIN PFP */

uint8_t AHT20_Init(void);
uint8_t AHT20_Read(float *temperature, float *humidity);

float ADC_To_Moisture(uint16_t adc);

UrgencyClass predict_urgency(
    float temperature_C,
    float humidity_pct,
    float soil_moisture_pct,
    float moisture_trend,
    float light_lux,
    float hour_of_day
);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* ============================================================
 * AHT20 INITIALIZATION
 * ============================================================
 */

uint8_t AHT20_Init(void)
{
    uint8_t command[3];
    uint8_t status;

    /*
     * Check AHT20 address 0x38.
     */
    if (HAL_I2C_IsDeviceReady(
            &hi2c1,
            AHT20_ADDR,
            3,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Read AHT20 status.
     */
    if (HAL_I2C_Mem_Read(
            &hi2c1,
            AHT20_ADDR,
            0x71,
            I2C_MEMADD_SIZE_8BIT,
            &status,
            1,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Initialize calibration if required.
     */
    if ((status & 0x08) == 0)
    {
        command[0] = 0xBE;
        command[1] = 0x08;
        command[2] = 0x00;

        if (HAL_I2C_Master_Transmit(
                &hi2c1,
                AHT20_ADDR,
                command,
                3,
                100) != HAL_OK)
        {
            return 0;
        }

        HAL_Delay(10);
    }

    return 1;
}


/* ============================================================
 * AHT20 READ
 * ============================================================
 */

uint8_t AHT20_Read(float *temperature, float *humidity)
{
    uint8_t command[3];
    uint8_t data[6];

    uint32_t rawHumidity;
    uint32_t rawTemperature;

    /*
     * Start measurement.
     */
    command[0] = 0xAC;
    command[1] = 0x33;
    command[2] = 0x00;

    if (HAL_I2C_Master_Transmit(
            &hi2c1,
            AHT20_ADDR,
            command,
            3,
            100) != HAL_OK)
    {
        return 0;
    }

    HAL_Delay(80);

    /*
     * Read measurement.
     */
    if (HAL_I2C_Master_Receive(
            &hi2c1,
            AHT20_ADDR,
            data,
            6,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Sensor busy?
     */
    if (data[0] & 0x80)
    {
        return 0;
    }

    /*
     * Humidity raw value.
     */
    rawHumidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)(data[3] >> 4));

    /*
     * Temperature raw value.
     */
    rawTemperature =
        ((uint32_t)(data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];

    /*
     * Convert humidity.
     */
    *humidity =
        ((float)rawHumidity * 100.0f) / 1048576.0f;

    /*
     * Convert temperature.
     */
    *temperature =
        (((float)rawTemperature * 200.0f) / 1048576.0f)
        - 50.0f;

    return 1;
}


/* ============================================================
 * SOIL MOISTURE
 * ============================================================
 *
 * Your original Micro Peta thresholds:
 *
 * ADC > 2300
 * ADC > 1800
 * ADC > 1500
 * otherwise
 *
 * Since your ML tree expects a percentage, each band is given
 * a representative percentage.
 *
 * These are NOT calibration values.
 * ============================================================
 */

float ADC_To_Moisture(uint16_t adc)
{
    if (adc > 2300)
    {
        return 12.5f;
    }
    else if (adc > 1800)
    {
        return 37.5f;
    }
    else if (adc > 1500)
    {
        return 62.5f;
    }
    else
    {
        return 87.5f;
    }
}


/* ============================================================
 * OUR DECISION TREE
 * ============================================================
 */

UrgencyClass predict_urgency(
    float temperature_C,
    float humidity_pct,
    float soil_moisture_pct,
    float moisture_trend,
    float light_lux,
    float hour_of_day)
{
    if (soil_moisture_pct <= 46.6250f)
    {
        if (soil_moisture_pct <= 45.9400f)
        {
            if (humidity_pct <= 50.1400f)
            {
                if (humidity_pct <= 50.0600f)
                {
                    return URGENCY_URGENT;
                }
                else
                {
                    return URGENCY_SOON;
                }
            }
            else
            {
                return URGENCY_URGENT;
            }
        }
        else
        {
            if (light_lux <= 190.7500f)
            {
                return URGENCY_SOON;
            }
            else
            {
                return URGENCY_URGENT;
            }
        }
    }
    else
    {
        if (soil_moisture_pct <= 68.1900f)
        {
            if (soil_moisture_pct <= 48.9700f)
            {
                if (light_lux <= 3906.4501f)
                {
                    return URGENCY_SOON;
                }
                else
                {
                    return URGENCY_URGENT;
                }
            }
            else
            {
                if (soil_moisture_pct <= 66.4250f)
                {
                    return URGENCY_SOON;
                }
                else
                {
                    if (humidity_pct <= 54.5350f)
                    {
                        return URGENCY_NOT_URGENT;
                    }
                    else
                    {
                        return URGENCY_SOON;
                    }
                }
            }
        }
        else
        {
            if (soil_moisture_pct <= 71.9150f)
            {
                if (temperature_C <= 28.4100f)
                {
                    return URGENCY_NOT_URGENT;
                }
                else
                {
                    if (humidity_pct <= 53.6000f)
                    {
                        return URGENCY_NOT_URGENT;
                    }
                    else
                    {
                        return URGENCY_SOON;
                    }
                }
            }
            else
            {
                if (soil_moisture_pct <= 72.4500f)
                {
                    if (soil_moisture_pct <= 72.4100f)
                    {
                        return URGENCY_NOT_URGENT;
                    }
                    else
                    {
                        return URGENCY_SOON;
                    }
                }
                else
                {
                    return URGENCY_NOT_URGENT;
                }
            }
        }
    }
}


/* USER CODE END 0 */


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();

  /* Initialize OLED */
  SSD1306_Init();

  /* Start ADC */
  HAL_ADC_Start(&hadc1);

  /*
   * Try AHT20.
   *
   * If it is not connected/working, aht20_ok = 0
   * and dummy temperature/humidity will be used.
   */
  aht20_ok = AHT20_Init();

  /*
   * Initial dummy values.
   */
  temperature_C = DUMMY_TEMPERATURE;
  humidity_pct = DUMMY_HUMIDITY;

  /* Infinite loop */
  while (1)
  {
    /* ========================================================
     * SOIL MOISTURE
     * ========================================================
     */

    HAL_ADC_PollForConversion(&hadc1, 1000);

    readValue = HAL_ADC_GetValue(&hadc1);

    soil_moisture_pct = ADC_To_Moisture(readValue);


    /* ========================================================
     * AHT20
     * ========================================================
     *
     * If available:
     *     real temperature
     *     real humidity
     *
     * Otherwise:
     *     dummy temperature
     *     dummy humidity
     */

    if (aht20_ok)
    {
        if (AHT20_Read(
                &temperature_C,
                &humidity_pct) == 0)
        {
            aht20_ok = 0;

            temperature_C = DUMMY_TEMPERATURE;
            humidity_pct = DUMMY_HUMIDITY;
        }
    }
    else
    {
        temperature_C = DUMMY_TEMPERATURE;
        humidity_pct = DUMMY_HUMIDITY;
    }


    /* ========================================================
     * OTHER ML INPUTS
     * ========================================================
     *
     * No light sensor is currently available.
     *
     * No RTC/hour input is currently available.
     *
     * moisture_trend and hour_of_day are not used anywhere
     * in your decision tree.
     */

    moisture_trend = DUMMY_TREND;
    light_lux = DUMMY_LIGHT;
    hour_of_day = DUMMY_HOUR;


    /* ========================================================
     * ML PREDICTION
     * ========================================================
     */

    urgency = predict_urgency(
        temperature_C,
        humidity_pct,
        soil_moisture_pct,
        moisture_trend,
        light_lux,
        hour_of_day
    );


    /* ========================================================
     * OLED DISPLAY
     * ========================================================
     *
     * No printf.
     * No sprintf.
     *
     * Only moisture and predicted urgency.
     */

    SSD1306_Clear();


    /* --------------------------------------------------------
     * MOISTURE
     * --------------------------------------------------------
     */

    SSD1306_GotoXY(0, 0);
    SSD1306_Puts(
        "MOISTURE",
        &Font_11x18,
        1
    );


    /*
     * Display the same four moisture bands used by the
     * original Micro Peta logic.
     */

    if (readValue > 2300)
    {
        SSD1306_GotoXY(0, 20);
        SSD1306_Puts(
            "0-25%",
            &Font_11x18,
            1
        );
    }
    else if (readValue > 1800)
    {
        SSD1306_GotoXY(0, 20);
        SSD1306_Puts(
            "26-50%",
            &Font_11x18,
            1
        );
    }
    else if (readValue > 1500)
    {
        SSD1306_GotoXY(0, 20);
        SSD1306_Puts(
            "51-75%",
            &Font_11x18,
            1
        );
    }
    else
    {
        SSD1306_GotoXY(0, 20);
        SSD1306_Puts(
            "76-100%",
            &Font_11x18,
            1
        );
    }


    /* --------------------------------------------------------
     * URGENCY
     * --------------------------------------------------------
     */

    SSD1306_GotoXY(0, 42);
    SSD1306_Puts(
        "URGENCY:",
        &Font_7x10,
        1
    );


    if (urgency == URGENCY_URGENT)
    {
        SSD1306_GotoXY(65, 42);
        SSD1306_Puts(
            "URGENT",
            &Font_7x10,
            1
        );
    }
    else if (urgency == URGENCY_SOON)
    {
        SSD1306_GotoXY(65, 42);
        SSD1306_Puts(
            "SOON",
            &Font_7x10,
            1
        );
    }
    else
    {
        SSD1306_GotoXY(65, 42);
        SSD1306_Puts(
            "OK",
            &Font_7x10,
            1
        );
    }


    SSD1306_UpdateScreen();

    HAL_Delay(500);
  }
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 |
      RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(
          &RCC_ClkInitStruct,
          FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief ADC1 Initialization Function
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;

  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;

  if (HAL_ADC_ConfigChannel(
          &hadc1,
          &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief I2C1 Initialization Function
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;

  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}


/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
}


/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
  }
}


#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}

#endif