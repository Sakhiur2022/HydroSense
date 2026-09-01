#include "sensors.h"
#include "stm32f1xx_hal.h"

// Declare the ADC and I2C handles (defined in main.c)
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;

// ============================================
//  AHT20 Temperature & Humidity Sensor (I2C)
// ============================================
#define AHT20_ADDR         0x38
#define AHT20_CMD_INIT     0xBE
#define AHT20_CMD_MEASURE  0xAC

static uint8_t AHT20_SendCommand(uint8_t cmd) {
    uint8_t data[1] = {cmd};
    return HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, data, 1, 100) == HAL_OK;
}

static uint8_t AHT20_ReadRaw(uint32_t *raw_temp, uint32_t *raw_hum) {
    uint8_t rx_buf[6];
    uint8_t cmd[3] = {AHT20_CMD_MEASURE, 0x33, 0x00};

    // Send measurement command
    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, cmd, 3, 100) != HAL_OK)
        return 0;

    HAL_Delay(80); // Measurement takes ~75-80ms

    // Read 6 bytes
    if (HAL_I2C_Master_Receive(&hi2c1, AHT20_ADDR << 1, rx_buf, 6, 100) != HAL_OK)
        return 0;

    // Check status bit (bit 7 of byte 0 should be 0 when ready)
    if (rx_buf[0] & 0x80)
        return 0;

    // Extract humidity (20 bits) and temperature (20 bits)
    uint32_t hum_raw = ((uint32_t)rx_buf[1] << 12) | ((uint32_t)rx_buf[2] << 4) | (rx_buf[3] >> 4);
    uint32_t temp_raw = ((uint32_t)(rx_buf[3] & 0x0F) << 16) | ((uint32_t)rx_buf[4] << 8) | rx_buf[5];

    *raw_hum = hum_raw;
    *raw_temp = temp_raw;
    return 1;
}

float AHT20_ReadTemperature(void) {
    uint32_t raw_temp, raw_hum;
    if (!AHT20_ReadRaw(&raw_temp, &raw_hum))
        return -100.0f;

    // Convert: Temp = (raw * 200 / 2^20) - 50
    return (raw_temp * 200.0f / 1048576.0f) - 50.0f;
}

float AHT20_ReadHumidity(void) {
    uint32_t raw_temp, raw_hum;
    if (!AHT20_ReadRaw(&raw_temp, &raw_hum))
        return -1.0f;

    // Humidity = raw * 100 / 2^20
    return raw_hum * 100.0f / 1048576.0f;
}

static uint8_t AHT20_Init(void) {
    HAL_Delay(40); // Power-up delay

    uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
    if (HAL_I2C_Master_Transmit(&hi2c1, AHT20_ADDR << 1, init_cmd, 3, 100) != HAL_OK)
        return 0;

    HAL_Delay(10);
    return 1;
}

// ============================================
//  BH1750 (GY-30) Digital Light Sensor (I2C)
// ============================================
// Default address 0x23 (if ADDR pin on module is connected to GND)
// If ADDR is connected to VCC, change to 0x5C
#define BH1750_ADDR         0x23
#define BH1750_CMD_PWR_DOWN 0x00
#define BH1750_CMD_PWR_ON   0x01
#define BH1750_CMD_RESET    0x07
#define BH1750_CMD_HRES_M2  0x11  // High Resolution Mode 2 (0.5 lux precision)

static uint8_t BH1750_SendCmd(uint8_t cmd) {
    return HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR << 1, &cmd, 1, 100) == HAL_OK;
}

static uint16_t BH1750_ReadData(void) {
    uint8_t rx_buf[2];
    if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR << 1, rx_buf, 2, 100) != HAL_OK) {
        return 0;
    }
    // Combine bytes: MSB first
    return (rx_buf[0] << 8) | rx_buf[1];
}

float BH1750_ReadLux(void) {
    // Send Power On command
    BH1750_SendCmd(BH1750_CMD_PWR_ON);
    HAL_Delay(10);

    // Send measurement command (High Res Mode 2)
    if (!BH1750_SendCmd(BH1750_CMD_HRES_M2)) {
        return -1.0f;
    }

    // Measurement takes ~120ms for high-res mode
    HAL_Delay(150);

    // Read 2 bytes of data
    uint16_t raw = BH1750_ReadData();
    if (raw == 0) {
        return -1.0f;
    }

    // For H-Resolution Mode 2, resolution is 0.5 lux per count
    // Lux = raw / 2.0
    float lux = raw / 2.0f;

    // Cap at reasonable max (just in case)
    if (lux > 65535) lux = 65535;

    return lux;
}

static uint8_t BH1750_Init(void) {
    BH1750_SendCmd(BH1750_CMD_PWR_ON);
    HAL_Delay(10);
    BH1750_SendCmd(BH1750_CMD_RESET);
    HAL_Delay(10);
    return 1;
}

// ============================================
//  Capacitive Soil Moisture Sensor (Analog)
// ============================================
float Soil_ReadMoisture(void) {
    // Start ADC conversion on PA0 (Channel 0)
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return -1.0f;
    }

    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // ================================================================
    //  CALIBRATION REQUIRED: Replace these values with YOUR sensor's
    //  readings in dry air (ADC_DRY) and submerged in water (ADC_WET).
    // ================================================================
    // Typical ranges for capacitive v1.2:
    //   - Dry soil / air:  ~3500 - 3800
    //   - Submerged in water: ~1500 - 1800
    const uint32_t ADC_DRY = 3500;
    const uint32_t ADC_WET = 1500;

    // Clamp to valid range
    if (adc_value >= ADC_DRY)
        return 0.0f;
    if (adc_value <= ADC_WET)
        return 100.0f;

    // Linear interpolation: 100% = wet, 0% = dry
    float moisture = 100.0f * (ADC_DRY - adc_value) / (ADC_DRY - ADC_WET);

    // Clamp just in case of floating point rounding
    if (moisture < 0.0f) moisture = 0.0f;
    if (moisture > 100.0f) moisture = 100.0f;

    return moisture;
}

// ============================================
//  Master Initialization
// ============================================
void Sensors_Init(void) {
    // ADC and I2C are already initialized by CubeMX (MX_ADC1_Init, MX_I2C1_Init)
    // Just initialize the sensors
    AHT20_Init();
    BH1750_Init();
}

// ============================================
//  Read All Sensors
// ============================================
uint8_t Sensors_ReadAll(SensorData_t *data) {
    data->temperature = AHT20_ReadTemperature();
    data->humidity = AHT20_ReadHumidity();
    data->soil_moisture = Soil_ReadMoisture();
    data->light_intensity = BH1750_ReadLux();

    // Validate all readings
    data->valid = 1;
    if (data->temperature < -10 || data->temperature > 60) data->valid = 0;
    if (data->humidity < 0 || data->humidity > 100) data->valid = 0;
    if (data->soil_moisture < 0 || data->soil_moisture > 100) data->valid = 0;
    if (data->light_intensity < 0) data->valid = 0; // BH1750 returns -1 on error

    return data->valid;
}
