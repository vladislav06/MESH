//
// Created by vm on 25.6.4.
//

#ifndef MESH_CODE_MAIN_H
#define MESH_CODE_MAIN_H

#include "stm32l0xx_hal.h"
#include "cc1101.h"


// Array of gpio external interrupt handlers
static void (*exti_callbacks[])(uint16_t gpio) ={
        cc1101_exti_callback,
};

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2);

void on_receive(void);


void transmitter(void);
void receiver(void);
/**
 * Will call each exti in exti_callbacks
 * @param gpio
 */
void HAL_GPIO_EXTI_Callback(uint16_t gpio);

#endif //MESH_CODE_MAIN_H
