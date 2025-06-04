//
// Created by vm on 25.6.4.
//
#include "utils.h"

static TIM_HandleTypeDef *delay_timer;
static CRC_HandleTypeDef *crc;
static RNG_HandleTypeDef *rng;

void utils_init(TIM_HandleTypeDef *htim, CRC_HandleTypeDef *hcrc, RNG_HandleTypeDef *hrng) {
    delay_timer = htim;
    crc = hcrc;
    HAL_TIM_Base_Start(delay_timer);
    rng = hrng;
}

void delay_micros(uint16_t delay) {
    __HAL_TIM_SET_COUNTER(delay_timer, 0);  // set the counter value a 0
    while (__HAL_TIM_GET_COUNTER(delay_timer) < delay);  // wait for the counter to reach the us input in the parameter
}

uint16_t utils_calc_crc(uint8_t *data, uint32_t size) {
    return HAL_CRC_Calculate(crc, (uint32_t *) data, size);
}

uint32_t utils_random_int() {
    uint32_t random;
    HAL_RNG_GenerateRandomNumber(rng, &random);
    return random;
}

