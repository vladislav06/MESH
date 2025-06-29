//
// Created by vm on 25.3.6.
//

#ifndef MESH_CODE_SENSOR_H
#define MESH_CODE_SENSOR_H

#include "stdint.h"
#include "cc1101.h"


#define SENSOR_CHANNEL_COUNT 5
#define SUBSCRIBER_COUNT 10


extern uint8_t sensor_place;
extern uint8_t sensor_sensorCh;
extern ADC_HandleTypeDef *adc;



extern uint8_t sensor_channels[SENSOR_CHANNEL_COUNT];

struct Subscriber {
    uint16_t id;
    uint8_t dataCh;
};

extern struct Subscriber subscribers[SUBSCRIBER_COUNT];

void sensor_init(struct cc1101 *cc, UART_HandleTypeDef *uart, ADC_HandleTypeDef *adc);

void sensor_send();

void sensor_send_data(uint16_t value, uint8_t channel);

void sensor_remove_from_subscribers(uint16_t id, uint8_t dataCh);

#endif //MESH_CODE_SENSOR_H
