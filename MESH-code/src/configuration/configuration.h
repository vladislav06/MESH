//
// Created by vm on 25.6.6.
//

#ifndef MESH_CODE_CONFIGURATION_H
#define MESH_CODE_CONFIGURATION_H
#include "stdint.h"

struct DataChannel{
    char name;
    uint8_t dataCh;
    uint8_t place;
    uint8_t sensorCh;
    uint8_t mixMode;
};

// uint8_t number of used dataChannels
// struct DataChannel*
// char* \0 ending formula string

#endif //MESH_CODE_CONFIGURATION_H
