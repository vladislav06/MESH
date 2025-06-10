//
// Created by vm on 25.17.5.
//

#ifndef MESH_CODE_EEPROM_H
#define MESH_CODE_EEPROM_H

#include "stdint.h"
/**
 * Pointer to eeprom data, can be accessed anywhere. Size of eeprom region is define in hw.h EEPROM_SIZE
 */
extern const uint8_t *EEPROM_DATA;

/**
 * Size of data must be multiple of 32 bytes
 */
void eeprom_store_large(uint8_t *data, uint32_t size, uint32_t offset);

void eeprom_store(uint8_t *data, uint32_t size, uint32_t offset);

#endif //MESH_CODE_EEPROM_H
