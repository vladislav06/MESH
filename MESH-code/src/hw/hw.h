//
// Created by vm on 25.1.5.
//

#ifndef MESH_CODE_HW_H
#define MESH_CODE_HW_H

#include "utils.h"

#define D3 GPIO_PIN_15
#define D4 GPIO_PIN_1

#define EEPROM_PTR 0x08080000
#define EEPROM_SIZE 2048

void hw_set_D4(bool state);

void hw_set_D3(bool state);

void hw_enable_ld(bool state);

uint16_t hw_id(void);

// interrupt stuff
#define INTERRUPT_AMOUNT 10

struct Interrupt {
    void (*interrupt)(uint16_t gpio, void *arg);

    void *arg;
    uint16_t gpio;
};

extern bool enableInterrupts;

void register_interrupt(struct Interrupt interrupt, int place);

/**
 * Will call each exti in exti_callbacks
 * @param gpio
 */
void HAL_GPIO_EXTI_Callback(uint16_t gpio);


#endif //MESH_CODE_HW_H
