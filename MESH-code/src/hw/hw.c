//
// Created by vm on 25.1.5.
//
#include "hw.h"
#include "eeprom.h"

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

/*
* stm32 has 96 bit large unique id which consists of
* UID[31:0]:  X and Y coordinates on the wafer expressed in BCD format
* UID[39:32]: WAF_NUM[7:0] Wafer number (8-bit unsigned number)
* UID[63:40]: LOT_NUM[23:0] Lot number (ASCII encoded)
* UID[95:64]: LOT_NUM[55:24] Lot number (ASCII encoded)
* 96 bits are equally split among 3 uint32
* Because of that and the fact that all 10 chips are from same batch, first 2 uint32 are the same,
* and does not require check
* example:
* 0  256324400
* 1  909523256
* 2  4128845
* 0  256324400
* 1  909523256
* 2  4128848
*/
uint16_t hw_id(void) {
    return ((HAL_GetUIDw2() & 0xff0000) >> 8) | (HAL_GetUIDw2() & 0xff);
}

void hw_init(ADC_HandleTypeDef *adc) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_RANK_NONE;
    HAL_ADC_ConfigChannel(adc, &sConfig);
}

uint32_t hw_read_analog(ADC_HandleTypeDef *adc, uint32_t pin) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_9;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    HAL_ADC_ConfigChannel(adc, &sConfig);
    HAL_ADC_Start(adc);
    HAL_ADC_PollForConversion(adc, 10);
    uint32_t val = HAL_ADC_GetValue(adc);
    sConfig.Rank = ADC_RANK_NONE;
    HAL_ADC_ConfigChannel(adc, &sConfig);
    HAL_ADC_Stop(adc);
    return val;
}

void hw_set_sensor_config(struct SensorConfig place) {
    eeprom_store((uint8_t *) &place, sizeof(struct SensorConfig), EEPROM_SIZE - sizeof(struct SensorConfig));
}

struct SensorConfig hw_get_sensor_config() {

    struct SensorConfig result = *(struct SensorConfig *) ((uint8_t *) (EEPROM_PTR + (EEPROM_SIZE -
                                                                                      sizeof(struct SensorConfig))));
    return result;
}

