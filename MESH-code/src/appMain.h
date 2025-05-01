//
// Created by vm on 25.6.4.
//

#ifndef MESH_CODE_MAIN_H
#define MESH_CODE_MAIN_H

#include "stm32l0xx_hal.h"
#include "cc1101.h"

struct Packet{
    uint32_t id;
    float rssi;
    uint32_t checksum;
};
struct Interrupt {
    void (*interrupt)(uint16_t gpio, void *arg);

    void *arg;
    uint16_t gpio;
    GPIO_TypeDef *port;
};

static uint8_t enableInterrupts = 0;
// Array of gpio external interrupt handlers
static struct Interrupt interrupts[] = {
        {},//place for cc1101 GD0 interrupt
        {},//place for cc1101 GD2 interrupt
};

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2);

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq);
void on_receive_empty(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq);

void transmitter(void);

void receiver(void);

/**
 * Will call each exti in exti_callbacks
 * @param gpio
 */
void HAL_GPIO_EXTI_Callback(uint16_t gpio);

#endif //MESH_CODE_MAIN_H
