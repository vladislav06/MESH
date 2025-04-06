//
// Created by vm on 25.6.4.
//
#include "utils.h"

static TIM_HandleTypeDef *delay_timer;

void utils_init(TIM_HandleTypeDef *htim) {
    delay_timer = htim;
    HAL_TIM_Base_Start(delay_timer);
}

void delay_micros(uint16_t delay) {
    __HAL_TIM_SET_COUNTER(delay_timer, 0);  // set the counter value a 0
    while (__HAL_TIM_GET_COUNTER(delay_timer) < delay);  // wait for the counter to reach the us input in the parameter
}
