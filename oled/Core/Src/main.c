/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Soil moisture + AHT20 + GY-30 + ML urgency
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

/* Private define 
/* USER CODE BEGIN PD */

/* AHT20 I2C address */
#define AHT20_ADDR (0x38 << 1)

/* GY-30 / BH1750 I2C address */
#define GY30_ADDR (0x23 << 1)

/* BH1750 continuous high resolution mode */
#define GY30_CONTINUOUS_HIGH_RES 0x10

/*
 * Fallback values.
 * Real sensor values are used when communication is working.
 */
#define TEST_TEMP 25.0f
#define TEST_HUMIDITY    50.0f
#define TEST_LIGHT       500.0f

/*
 * Other ML inputs currently have no physical sensor.
 */
#define TEST_TREND       0.0f
#define TEST_HOUR        12.0f

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
uint8_t gy30_ok;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);

/* USER CODE BEGIN PFP */

uint8_t AHT20_Init(void);
uint8_t AHT20_Read(float *temperature, float *humidity);

uint8_t GY30_Init(void);
uint8_t GY30_Read(float *light_lux);

float ADC_To_Moisture(uint16_t adc);

UrgencyClass predict_urgency(
    float temperature_C,
    float humidity_pct,
    float soil_moisture_pct,
    float moisture_trend,
    float light_lux,
    float hour_of_day
);

/* OLED number functions */

void OLED_Print_Number(uint16_t number);
void OLED_Print_Temperature(float temperature);
void OLED_Print_Humidity(float humidity);
void OLED_Print_Light(float light);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* 
 * AHT20 INITIALIZATION
 *
 */

uint8_t AHT20_Init(void)
{
    uint8_t command;
    uint8_t status;
    uint8_t init_command[3];

    /*
     * Check whether AHT20 responds.
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
     * AHT20 status command.
     *
     * Command = 0x71
     */
    command = 0x71;

    if (HAL_I2C_Master_Transmit(
            &hi2c1,
            AHT20_ADDR,
            &command,
            1,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Receive status byte.
     */
    if (HAL_I2C_Master_Receive(
            &hi2c1,
            AHT20_ADDR,
            &status,
            1,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Calibration enable bit = bit 3.
     */
    if ((status & 0x08) == 0)
    {
        init_command[0] = 0xBE;
        init_command[1] = 0x08;
        init_command[2] = 0x00;

        if (HAL_I2C_Master_Transmit(
                &hi2c1,
                AHT20_ADDR,
                init_command,
                3,
                100) != HAL_OK)
        {
            return 0;
        }

        HAL_Delay(10);
    }

    return 1;
}


/* 
 * AHT20 READ TEMPERATURE + HUMIDITY
 * 
 */

uint8_t AHT20_Read(float *temperature, float *humidity)
{
    uint8_t command[3];
    uint8_t data[6];

    uint32_t rawHumidity;
    uint32_t rawTemperature;

    /*
     * Start measurement.
     *
     * 0xAC = trigger measurement
     * 0x33 = measurement configuration
     * 0x00
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

    /*
     * Wait for measurement.
     */
    HAL_Delay(80);

    /*
     * Read 6 bytes.
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
     * Bit 7 of status byte indicates BUSY.
     */
    if (data[0] & 0x80)
    {
        return 0;
    }

    /*
     * Extract 20-bit humidity value.
     */
    rawHumidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)(data[3] >> 4));

    /*
     * Extract 20-bit temperature value.
     */
    rawTemperature =
        ((uint32_t)(data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];

    /*
     * Convert humidity to percentage.
     */
    *humidity =
        ((float)rawHumidity * 100.0f) / 1048576.0f;

    /*
     * Convert temperature to Celsius.
     */
    *temperature =
        (((float)rawTemperature * 200.0f) / 1048576.0f)
        - 50.0f;

    return 1;
}


/* 
 * GY-30 / BH1750 INITIALIZATION
 * 
 */

uint8_t GY30_Init(void)
{
    uint8_t command;

    /*
     * Check whether GY-30 responds.
     *
     * Default BH1750 address = 0x23.
     */
    if (HAL_I2C_IsDeviceReady(
            &hi2c1,
            GY30_ADDR,
            3,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Start continuous high-resolution measurement.
     *
     * BH1750 command = 0x10
     */
    command = GY30_CONTINUOUS_HIGH_RES;

    if (HAL_I2C_Master_Transmit(
            &hi2c1,
            GY30_ADDR,
            &command,
            1,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Wait for the first measurement.
     */
    HAL_Delay(180);

    return 1;
}


/* 
 * GY-30 / BH1750 READ LIGHT
 * 
 */

uint8_t GY30_Read(float *light_lux)
{
    uint8_t data[2];
    uint16_t rawLight;

    /*
     * Read 16-bit measurement.
     */
    if (HAL_I2C_Master_Receive(
            &hi2c1,
            GY30_ADDR,
            data,
            2,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Combine high byte and low byte.
     */
    rawLight =
        ((uint16_t)data[0] << 8) |
        data[1];

    /*
     * BH1750 lux conversion.
     */
    *light_lux =
        (float)rawLight / 1.2f;

    return 1;
}


/* 
 * SOIL MOISTURE

 * ADC > 2300
 * ADC > 1800
 * ADC > 1500
 * otherwise
 *
 * Representative ML values:
 *
 * >2300       = 12.5%
 * 1801-2300  = 37.5%
 * 1501-1800  = 62.5%
 * <=1500     = 87.5%
 
 * 
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
 * NUMBER PRINTING
 * ============================================================
 *
 * No printf()
 * No sprintf()
 * No stdio.h
 * ============================================================
 */

void OLED_Print_Number(uint16_t number)
{
    char digits[5];
    char digit[2];

    uint8_t i;

    i = 0;

    /*
     * Special case for zero.
     */
    if (number == 0)
    {
        digit[0] = '0';
        digit[1] = '\0';

        SSD1306_Puts(
            digit,
            &Font_7x10,
            1
        );

        return;
    }

    /*
     * Convert number into characters.
     */
    while (number > 0)
    {
        digits[i] = (number % 10) + '0';
        number = number / 10;

        i++;
    }

    /*
     * Print in reverse order.
     */
    while (i > 0)
    {
        i--;

        digit[0] = digits[i];
        digit[1] = '\0';

        SSD1306_Puts(
            digit,
            &Font_7x10,
            1
        );
    }
}


/* ============================================================
 * TEMPERATURE DISPLAY
 * ============================================================
 */

void OLED_Print_Temperature(float temperature)
{
    uint16_t temp;

    if (temperature < 0)
    {
        SSD1306_Puts(
            "-",
            &Font_7x10,
            1
        );

        temp = (uint16_t)((-temperature) + 0.5f);
    }
    else
    {
        temp = (uint16_t)(temperature + 0.5f);
    }

    OLED_Print_Number(temp);

    SSD1306_Puts(
        "C",
        &Font_7x10,
        1
    );
}


/* ============================================================
 * HUMIDITY DISPLAY
 * ============================================================
 */

void OLED_Print_Humidity(float humidity)
{
    uint16_t hum;

    if (humidity < 0)
    {
        humidity = 0;
    }

    if (humidity > 100)
    {
        humidity = 100;
    }

    hum = (uint16_t)(humidity + 0.5f);

    OLED_Print_Number(hum);

    SSD1306_Puts(
        "%",
        &Font_7x10,
        1
    );
}


/* ============================================================
 * LIGHT DISPLAY
 * ============================================================
 */

void OLED_Print_Light(float light)
{
    uint16_t lux;

    if (light < 0)
    {
        light = 0;
    }

    if (light > 65535.0f)
    {
        light = 65535.0f;
    }

    lux = (uint16_t)(light + 0.5f);

    OLED_Print_Number(lux);

    SSD1306_Puts(
        "lx",
        &Font_7x10,
        1
    );
}


/* ============================================================
 *  TRAINED DECISION TREE
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
    
    (void)moisture_trend;
    (void)hour_of_day;

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
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();

    /* USER CODE BEGIN 2 */

    /*
     * OLED initialization.
     */
    SSD1306_Init();

    /*
     * AHT20 initialization.
     */
    aht20_ok = AHT20_Init();

    /*
     * GY-30 initialization.
     */
    gy30_ok = GY30_Init();

    /*
     * Initial values.
     */
    temperature_C = TEST_TEMP;
    humidity_pct = TEST_HUMIDITY;
    light_lux = TEST_LIGHT;

    moisture_trend = TEST_TREND;
    hour_of_day = TEST_HOUR;

    /*
     * IMPORTANT:
     *
     * ADC continuous conversion is DISABLED.
     *
     * Therefore ADC_Start() is called for every
     * individual soil measurement inside the loop.
     */

    /* USER CODE END 2 */

    /* Infinite loop */

    /* USER CODE BEGIN WHILE */

    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */


        /* ========================================================
         * SOIL MOISTURE
         * ========================================================
         *
         * Continuous conversion = DISABLED
         *
         * One reading:
         *
         * Start
         * Poll
         * GetValue
         * Stop
         */

        if (HAL_ADC_Start(&hadc1) == HAL_OK)
        {
            if (HAL_ADC_PollForConversion(
                    &hadc1,
                    1000) == HAL_OK)
            {
                readValue = HAL_ADC_GetValue(&hadc1);

                soil_moisture_pct =
                    ADC_To_Moisture(readValue);
            }

            HAL_ADC_Stop(&hadc1);
        }


        /* ========================================================
         * AHT20
         * ========================================================
         */

        if (aht20_ok)
        {
            if (AHT20_Read(
                    &temperature_C,
                    &humidity_pct) == 0)
            {
                /*
                 * Keep the previous valid value.
                 */
            }
        }
        else
        {
            /*
             * Try to reconnect.
             */
            aht20_ok = AHT20_Init();
        }


        /* 
         * GY-30
         * 
         */

        if (gy30_ok)
        {
            if (GY30_Read(&light_lux) == 0)
            {
                /*
                 * Keep the previous valid light value.
                 */
            }
        }
        else
        {
            /*
             * Try to reconnect.
             */
            gy30_ok = GY30_Init();
        }


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
         * OLED
         * ========================================================
         *
         * Line 1 = Temperature
         * Line 2 = Humidity
         * Line 3 = Moisture
         * Line 4 = Light
         * Line 5 = Urgency
         */

        SSD1306_Fill(0);


        /* --------------------------------------------------------
         * TEMPERATURE
         * --------------------------------------------------------
         */

        SSD1306_GotoXY(0, 0);

        SSD1306_Puts(
            "T:",
            &Font_7x10,
            1
        );

        OLED_Print_Temperature(temperature_C);


        /* --------------------------------------------------------
         * HUMIDITY
         * --------------------------------------------------------
         */

        SSD1306_GotoXY(0, 12);

        SSD1306_Puts(
            "H:",
            &Font_7x10,
            1
        );

        OLED_Print_Humidity(humidity_pct);


        /* --------------------------------------------------------
         * SOIL MOISTURE
         * --------------------------------------------------------
         */

        SSD1306_GotoXY(0, 24);

        SSD1306_Puts(
            "MOIST:",
            &Font_7x10,
            1
        );

        /*
         * Four original soil moisture bands.
         */
        if (soil_moisture_pct <= 25.0f)
        {
            SSD1306_Puts(
                "0-25%",
                &Font_7x10,
                1
            );
        }
        else if (soil_moisture_pct <= 50.0f)
        {
            SSD1306_Puts(
                "26-50%",
                &Font_7x10,
                1
            );
        }
        else if (soil_moisture_pct <= 75.0f)
        {
            SSD1306_Puts(
                "51-75%",
                &Font_7x10,
                1
            );
        }
        else
        {
            SSD1306_Puts(
                "76-100%",
                &Font_7x10,
                1
            );
        }


        /* --------------------------------------------------------
         * LIGHT
         * --------------------------------------------------------
         */

        SSD1306_GotoXY(0, 36);

        SSD1306_Puts(
            "LIGHT:",
            &Font_7x10,
            1
        );

        OLED_Print_Light(light_lux);


        /* --------------------------------------------------------
         * URGENCY
         * --------------------------------------------------------
         */

        SSD1306_GotoXY(0, 48);

        SSD1306_Puts(
            "URG:",
            &Font_7x10,
            1
        );

        if (urgency == URGENCY_NOT_URGENT)
        {
            SSD1306_Puts(
                "NOT URGENT",
                &Font_7x10,
                1
            );
        }
        else if (urgency == URGENCY_SOON)
        {
            SSD1306_Puts(
                "SOON",
                &Font_7x10,
                1
            );
        }
        else
        {
            SSD1306_Puts(
                "URGENT",
                &Font_7x10,
                1
            );
        }


        SSD1306_UpdateScreen();


        /*
         * Update every 500 ms.
         */
        HAL_Delay(500);
    }

    /* USER CODE END 3 */
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

    /** Initializes the RCC Oscillators
      */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(
            &RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
      */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection =
        RCC_PERIPHCLK_ADC;

    PeriphClkInit.AdcClockSelection =
        RCC_ADCPCLK2_DIV2;

    if (HAL_RCCEx_PeriphCLKConfig(
            &PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC1_Init 0 */

    /* USER CODE END ADC1_Init 0 */

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */

    /** Common config
      */

    hadc1.Instance = ADC1;

    hadc1.Init.ScanConvMode =
        ADC_SCAN_DISABLE;

    /*
     * CONTINUOUS READING DISABLED
     */
    hadc1.Init.ContinuousConvMode =
        DISABLE;

    hadc1.Init.DiscontinuousConvMode =
        DISABLE;

    hadc1.Init.ExternalTrigConv =
        ADC_SOFTWARE_START;

    hadc1.Init.DataAlign =
        ADC_DATAALIGN_RIGHT;

    hadc1.Init.NbrOfConversion =
        1;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Regular Channel
      */

    sConfig.Channel =
        ADC_CHANNEL_9;

    sConfig.Rank =
        ADC_REGULAR_RANK_1;

    sConfig.SamplingTime =
        ADC_SAMPLETIME_1CYCLE_5;

    if (HAL_ADC_ConfigChannel(
            &hadc1,
            &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN ADC1_Init 2 */

    /* USER CODE END ADC1_Init 2 */
}


/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
    /* USER CODE BEGIN I2C1_Init 0 */

    /* USER CODE END I2C1_Init 0 */

    /* USER CODE BEGIN I2C1_Init 1 */

    /* USER CODE END I2C1_Init 1 */

    hi2c1.Instance = I2C1;

    hi2c1.Init.ClockSpeed =
        400000;

    hi2c1.Init.DutyCycle =
        I2C_DUTYCYCLE_2;

    hi2c1.Init.OwnAddress1 =
        0;

    hi2c1.Init.AddressingMode =
        I2C_ADDRESSINGMODE_7BIT;

    hi2c1.Init.DualAddressMode =
        I2C_DUALADDRESS_DISABLE;

    hi2c1.Init.OwnAddress2 =
        0;

    hi2c1.Init.GeneralCallMode =
        I2C_GENERALCALL_DISABLE;

    hi2c1.Init.NoStretchMode =
        I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }

    /* USER CODE BEGIN I2C1_Init 2 */

    /* USER CODE END I2C1_Init 2 */
}


/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}


/* USER CODE BEGIN 4 */

/* USER CODE END 4 */


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */

    __disable_irq();

    while (1)
    {
    }

    /* USER CODE END Error_Handler_Debug */
}


#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  * @param  file: pointer to the source file name
  * @param  line: assert_param error source line number
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */

    /*
     * No printf() or sprintf().
     */

    (void)file;
    (void)line;

    /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */