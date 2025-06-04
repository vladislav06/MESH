//
// Created by vm on 25.6.4.
//

//#define bool uint8_t
//#define true 1
//#define false 0
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#ifndef MESH_CODE_DELAY_MICROS_H
#define MESH_CODE_DELAY_MICROS_H

#include "stm32l0xx_hal.h"


void utils_init(TIM_HandleTypeDef *htim, CRC_HandleTypeDef *hcrc,RNG_HandleTypeDef *hrng);

void delay_micros(uint16_t delay);

uint16_t utils_calc_crc(uint8_t *data, uint32_t size);

uint32_t utils_random_int();

#endif //MESH_CODE_DELAY_MICROS_H
