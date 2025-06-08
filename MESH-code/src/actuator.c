//
// Created by vm on 25.21.5.
//

#include "actuator.h"

#define DEBUG

#include "stdio.h"

#include "expr.h"
#include "eeprom.h"
#include "sensor.h"
#include "configuration.h"
#include "wirelessComms.h"

uint8_t placePosInEEPROM;
struct expr* expression;

/// Public functions
void actuator_load_config() {
    // Get current place
    const uint8_t intendedPlace = sensor_place;
    // 0 and 1 hold 0x6996 (garbage-saving purposes), 2 holds the placeCount
    const uint8_t placeCount = EEPROM_DATA[2];

    // Iterating through the places
    uint16_t bytePos = 3;
    uint8_t skippedPlaceCount = 0;
    placePosInEEPROM = 0;
    while (skippedPlaceCount < placeCount) {
        // Check whether this place is the one we want
        if (EEPROM_DATA[bytePos]== intendedPlace) {
            placePosInEEPROM = bytePos;
            break;
        }

        // Skip through usedChannels
        bytePos++;
        uint8_t usedChannelCount = EEPROM_DATA[bytePos];
        bytePos += usedChannelCount * sizeof(struct DataChannel);
        // Skip through algorithm
        uint16_t algoLength = EEPROM_DATA[bytePos];
        bytePos += algoLength * sizeof(char);

        // Onto the next one
        skippedPlaceCount++;
    }

    if (placePosInEEPROM == 0) {
        return;
    }

    // Reuse bytePos to inspect the found place - initially points to "place"
    bytePos = placePosInEEPROM;

    // Read usedChannelCount
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    // Read channels
    bytePos++;
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel* channel = (struct DataChannel *)&EEPROM_DATA[bytePos];
        // Subscribe to channel based on data provided
        try_subscribe(channel->place, channel->sensorCh, channel->dataCh);
        // Point to next channel
        bytePos += sizeof(struct DataChannel);
    }

    // Read algo - bytePos should already point to algoLength
    const uint16_t algoLength = EEPROM_DATA[bytePos];
    bytePos++;

    // TODO: Implement real functions for user_funcs later.
    static struct expr_func user_funcs[] = {
        {"", NULL, NULL, 0},
    };

    expression = expr_create(&EEPROM_DATA[bytePos], algoLength, user_funcs);
}

void actuator_handle_CD(struct PacketCD *pck) {
    LOG("CD was received from:%d dataCh: %d place: %d from: %d  =  %d",
        pck->header.originalSource, pck->dataCh,
        pck->place, pck->sensorCh, pck->value);
}
