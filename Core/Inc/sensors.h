#ifndef SENSORS_H
#define SENSORS_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct {
    float temperature;      // Celsius (from AHT20)
    float humidity;         // Percentage (from AHT20)
    float soil_moisture;    // Percentage (0-100) from capacitive sensor
    float light_intensity;  // Lux (from BH1750 / GY-30)
    uint8_t valid;          // 1 = all readings valid, 0 = any sensor error
} SensorData_t;

// Initialize all sensors (AHT20 and BH1750)
void Sensors_Init(void);

// Read all sensors in one shot
// Returns: 1 if all readings valid, 0 if any sensor failed
uint8_t Sensors_ReadAll(SensorData_t *data);

// Individual sensor read functions (for debugging)
float AHT20_ReadTemperature(void);
float AHT20_ReadHumidity(void);
float Soil_ReadMoisture(void);
float BH1750_ReadLux(void);

#endif
