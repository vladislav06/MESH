//
// Created by vm on 25.3.6.
//

#include "sensor.h"

// 1 by default
uint8_t sensor_place = 1;
uint8_t sensor_sensorCh = 1;
// data will be posted to channels 1 and 2
uint8_t sensor_dataChannels[DATA_CHANNEL_COUNT] = {1, 2};


struct Subscriber subscribers[SUBSCRIBER_COUNT] = {0};
