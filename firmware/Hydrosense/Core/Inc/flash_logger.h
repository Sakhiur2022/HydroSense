#ifndef FLASH_LOGGER_H
#define FLASH_LOGGER_H

#include <stdint.h>
#include "sensors.h"

#define LOG_PAGE_START    ((uint32_t)0x0800FC00)  // Last 1KB of 64KB flash (for F103C8)
#define LOG_PAGE_END      ((uint32_t)0x0800FFFF)  // End of flash
#define MAX_ENTRIES       128                     // Each entry is 8 bytes

typedef struct {
    uint32_t timestamp_seconds;  // Unix timestamp (or RTC counter)
    uint16_t temperature_x10;    // e.g., 26.5°C -> 265
    uint16_t humidity_x10;       // e.g., 55.3% -> 553
    uint16_t soil_moisture_x10;  // e.g., 78.2% -> 782
    uint16_t light_lux;          // e.g., 450 lux -> 450
} LogEntry_t;

// Initialize logger (erases log area if needed)
void Logger_Init(void);

// Add one log entry
// Returns: 1 = success, 0 = flash full or write error
uint8_t Logger_AddEntry(LogEntry_t *entry);

// Dump all logs over serial (UART)
void Logger_DumpAll(void);

// Get number of stored entries
uint16_t Logger_GetCount(void);

// Clear all logs (erase flash page)
void Logger_Clear(void);

#endif
