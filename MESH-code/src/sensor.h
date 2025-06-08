//
// Created by vm on 25.3.6.
//

#ifndef MESH_CODE_SENSOR_H
#define MESH_CODE_SENSOR_H

#include "stdint.h"

#define DATA_CHANNEL_COUNT 2
#define SUBSCRIBER_COUNT 10


extern uint8_t sensor_place;
extern uint8_t sensor_sensorCh;

extern uint8_t sensor_dataChannels[DATA_CHANNEL_COUNT];

struct Subscriber {
    uint16_t id;
    uint8_t dataCh;
};

extern struct Subscriber subscribers[SUBSCRIBER_COUNT];

#endif //MESH_CODE_SENSOR_H
