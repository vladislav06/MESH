//
// Created by vm on 25.17.5.
//

#include "eeprom.h"


const uint8_t *EEPROM_DATA = (uint8_t *) EEPROM_PTR;

/**
 * Size of data must be multiple of 32 bytes
 */
void eeprom_store(uint8_t *data, uint32_t size) {
    HAL_FLASHEx_DATAEEPROM_Unlock();
    FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);

    struct mem {
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
        uint32_t a1;
        uint32_t b1;
        uint32_t c1;
        uint32_t d1;
    };
    uint32_t *eepromStart = (uint32_t *) EEPROM_PTR;

    for (uint32_t i = 0; i < size / 32; i++) {
        *(__IO struct mem *) ((struct mem *) eepromStart + i) = ((struct mem *) data)[i];
    }

    FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);
    HAL_FLASHEx_DATAEEPROM_Lock();
}
