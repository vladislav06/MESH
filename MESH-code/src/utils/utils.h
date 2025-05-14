//
// Created by vm on 25.6.4.
//

#ifndef MESH_CODE_DELAY_MICROS_H
#define MESH_CODE_DELAY_MICROS_H

#include "stm32l0xx_hal.h"

#define bool uint8_t
#define true 1
#define false 0


void utils_init(TIM_HandleTypeDef *htim, UART_HandleTypeDef *uart);

void delay_micros(uint16_t delay);

#endif //MESH_CODE_DELAY_MICROS_H
