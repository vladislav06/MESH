//
// Created by vm on 25.6.4.
//

#ifndef MESH_CODE_MAIN_H
#define MESH_CODE_MAIN_H

#include "stm32l0xx_hal.h"
#include "cc1101.h"


void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2,
             CRC_HandleTypeDef *hcrc,
             RNG_HandleTypeDef *hrng);

#endif //MESH_CODE_MAIN_H
