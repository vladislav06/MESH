//
// Created by vm on 25.1.5.
//

#ifndef MESH_CODE_HW_H
#define MESH_CODE_HW_H

#include "utils.h"
#include "sensor.h"

#define D3 GPIO_PIN_15
#define D4 GPIO_PIN_1

#define EEPROM_PTR 0x08080000
#define EEPROM_SIZE 2048

void hw_init(ADC_HandleTypeDef *adc);

void hw_set_D4(bool state);

void hw_set_D3(bool state);

uint32_t hw_read_analog(ADC_HandleTypeDef *adc, uint32_t pin);

void hw_enable_ld(bool state);

uint16_t hw_id(void);

struct SensorConfig {
    uint8_t place;
    uint8_t sensor_ch;
    uint8_t mapping[DATA_CHANNEL_COUNT];
};

void hw_set_sensor_config(struct SensorConfig place);

struct SensorConfig hw_get_sensor_config();


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
