//
// Created by vm on 25.1.5.
//
#include "hw.h"

void hw_set_D4(bool state) {
    HAL_GPIO_WritePin(GPIOA, D4, state);
}

void hw_set_D3(bool state) {
    HAL_GPIO_WritePin(GPIOA, D3, state);
}

// interrupt stuff
bool enableInterrupts = false;

// Array of gpio external interrupt handlers
static struct Interrupt interrupts[INTERRUPT_AMOUNT] = {0};

void register_interrupt(struct Interrupt interrupt, int place) {
    if (place >= INTERRUPT_AMOUNT) {
        return;
    }
    interrupts[place] = interrupt;
}


void HAL_GPIO_EXTI_Callback(uint16_t gpio) {
    if (!enableInterrupts) {
        __HAL_GPIO_EXTI_CLEAR_IT(gpio);
        return;
    }
    for (uint32_t i = 0; i < (sizeof(interrupts) / sizeof(interrupts[0])); i++) {
        if (interrupts[i].gpio == gpio && interrupts[i].interrupt != 0) {
            interrupts[i].interrupt(gpio, interrupts[i].arg);
            //clear all pending interrupts
            // WARNING: might cause receive or transmit bugs
            __HAL_GPIO_EXTI_CLEAR_IT(gpio);
        }
    }
}

void hw_enable_ld(bool state) {

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, !state);

}

uint16_t hw_id(void) {
    return ((HAL_GetUIDw2() & 0xff0000) >> 8) | (HAL_GetUIDw2() & 0xff);
}

