#include "flash_logger.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>   // For printf

// Declare the UART handle (defined in main.c)
extern UART_HandleTypeDef huart1;

static uint16_t entry_count = 0;
static uint32_t next_write_addr = LOG_PAGE_START;

// ============ Flash Write Helpers ============
static void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

static void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

static uint8_t Flash_WriteHalfWord(uint32_t address, uint16_t data) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, data) != HAL_OK) {
        return 0;
    }
    return 1;
}

static void Flash_ErasePage(void) {
    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = LOG_PAGE_START,
        .NbPages = 1,
    };

    uint32_t page_error = 0;
    HAL_FLASHEx_Erase(&erase_init, &page_error);
}

// ============ Logger Functions ============
void Logger_Init(void) {
    // Check if first 2 bytes are magic marker 0xAAAA
    // If not, erase and initialize
    uint16_t magic = *(uint16_t*)LOG_PAGE_START;

    if (magic != 0xAAAA) {
        Flash_Unlock();
        Flash_ErasePage();
        Flash_WriteHalfWord(LOG_PAGE_START, 0xAAAA);  // Magic marker
        Flash_WriteHalfWord(LOG_PAGE_START + 2, 0);    // Entry count = 0
        Flash_Lock();
        entry_count = 0;
        next_write_addr = LOG_PAGE_START + 4;
    } else {
        // Read entry count from location +2
        entry_count = *(uint16_t*)(LOG_PAGE_START + 2);
        next_write_addr = LOG_PAGE_START + 4 + (entry_count * 8);

        // If count corrupted or exceeded, reset
        if (entry_count > MAX_ENTRIES || next_write_addr >= LOG_PAGE_END) {
            Logger_Clear();
        }
    }
}

uint8_t Logger_AddEntry(LogEntry_t *entry) {
    if (entry_count >= MAX_ENTRIES) {
        return 0;  // Flash full
    }

    // Check if we have space
    if ((next_write_addr + 8) >= LOG_PAGE_END) {
        return 0;
    }

    Flash_Unlock();

    // Write 4 half-words (8 bytes total)
    uint8_t success = 1;
    success &= Flash_WriteHalfWord(next_write_addr,      (uint16_t)(entry->timestamp_seconds & 0xFFFF));
    success &= Flash_WriteHalfWord(next_write_addr + 2,  (uint16_t)(entry->timestamp_seconds >> 16));
    success &= Flash_WriteHalfWord(next_write_addr + 4,  entry->temperature_x10);
    success &= Flash_WriteHalfWord(next_write_addr + 6,  entry->humidity_x10);
    success &= Flash_WriteHalfWord(next_write_addr + 8,  entry->soil_moisture_x10);
    success &= Flash_WriteHalfWord(next_write_addr + 10, entry->light_lux);

    if (success) {
        // Update entry count
        entry_count++;
        Flash_WriteHalfWord(LOG_PAGE_START + 2, entry_count);
        next_write_addr += 12;  // 6 half-words
    }

    Flash_Lock();
    return success;
}

uint16_t Logger_GetCount(void) {
    return entry_count;
}

void Logger_Clear(void) {
    Flash_Unlock();
    Flash_ErasePage();
    Flash_WriteHalfWord(LOG_PAGE_START, 0xAAAA);
    Flash_WriteHalfWord(LOG_PAGE_START + 2, 0);
    Flash_Lock();
    entry_count = 0;
    next_write_addr = LOG_PAGE_START + 4;
}

void Logger_DumpAll(void) {
    if (entry_count == 0) {
        printf("No entries to dump.\n");
        return;
    }

    printf("=== STM32 Soil Logger Dump ===\n");
    printf("Entries: %d\n", entry_count);
    printf("Timestamp,Temp(C),Humidity(%%),Soil(%%),Light(lux)\n");

    uint32_t addr = LOG_PAGE_START + 4;

    for (uint16_t i = 0; i < entry_count; i++) {
        uint32_t ts_low = *(uint16_t*)addr;
        uint32_t ts_high = *(uint16_t*)(addr + 2);
        uint32_t timestamp = (ts_high << 16) | ts_low;

        uint16_t temp = *(uint16_t*)(addr + 4);
        uint16_t hum = *(uint16_t*)(addr + 6);
        uint16_t soil = *(uint16_t*)(addr + 8);
        uint16_t light = *(uint16_t*)(addr + 10);

        printf("%lu,%d.%d,%d.%d,%d.%d,%d\n",
               timestamp,
               temp / 10, temp % 10,
               hum / 10, hum % 10,
               soil / 10, soil % 10,
               light);

        addr += 12;
    }
    printf("=== End Dump ===\n");
}
