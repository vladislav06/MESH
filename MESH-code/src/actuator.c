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

uint16_t placePosInEEPROM;
struct expr *expression;
uint16_t vars[10];

/// Public functions
void actuator_load_config() {
    // Get current place
    const uint8_t intendedPlace = sensor_place;
    // 0 and 1 hold 0x6996 (garbage-saving purposes), 2 holds the placeCount
    const uint8_t placeCount = EEPROM_DATA[2];

    // Reset inner variables
    placePosInEEPROM = 0;
    expr_destroy_expr();
    memset(vars, 0, 10);

    // Iterating through the places
    uint16_t bytePos = 3;
    uint8_t skippedPlaceCount = 0;
    while (skippedPlaceCount < placeCount) {
        // Check whether this place is the one we want
        if (EEPROM_DATA[bytePos] == intendedPlace) {
            placePosInEEPROM = bytePos;
            break;
        }

        // Skip through usedChannels
        bytePos++;
        uint8_t usedChannelCount = EEPROM_DATA[bytePos];
        bytePos += usedChannelCount * sizeof(struct DataChannel);
        // Skip through algorithm and its length
        uint16_t algoLength = EEPROM_DATA[bytePos] + (EEPROM_DATA[bytePos + 1] << 8);
        bytePos += 2;
        bytePos += algoLength * sizeof(char);

        // Onto the next one
        skippedPlaceCount++;
    }

    if (placePosInEEPROM == 0) {
        return;
    }

    // Reuse bytePos to inspect the found place - initially points to "place"
    bytePos = placePosInEEPROM;
    //skip usedChannelCount and usedChannels
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    // Read channels
    bytePos++;
    bytePos += usedChannelCount * sizeof(struct DataChannel);

    // Read algo - bytePos should already point to algoLength
    const uint16_t algoLength = EEPROM_DATA[bytePos];
    bytePos++;

    // TODO: Implement real functions for user_funcs later.
    static struct expr_func user_funcs[] = {
            {"", NULL, NULL, 0},
    };

    expression = expr_create(&EEPROM_DATA[bytePos], algoLength, user_funcs);
}

void actuator_subscribe() {
    uint16_t bytePos = placePosInEEPROM;

    // Read usedChannelCount
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    // Read channels
    bytePos++;
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel *channel = (struct DataChannel *) &EEPROM_DATA[bytePos];
        // Subscribe to channel based on data provided
        try_subscribe(channel->place, channel->sensorCh, channel->dataCh);
        // Point to next channel
        bytePos += sizeof(struct DataChannel);
    }
}


void actuator_handle_CD(struct PacketCD *pck) {
    LOG("CD was received from:%d dataCh: %d place: %d from: %d  =  %d",
        pck->header.originalSource, pck->dataCh,
        pck->place, pck->sensorCh, pck->value);

    // Iterate through places
    uint16_t bytePos = placePosInEEPROM;
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel *channel = (struct DataChannel *) &EEPROM_DATA[bytePos];
        // If this packet's info matches what we expect
        if (channel->dataCh == pck->dataCh &&
            (channel->place == pck->place || channel->place == 0) &&
            (channel->sensorCh == pck->sensorCh || channel->sensorCh == 0)) {
            // Then use our inner array - at place i in usedChannels is the channel that matches
            // i.e. channel that matches is at index 0 in usedChannels? then value for it is in vars at index 0, too
            vars[i] = pck->value;
            break;
        }
        // Keep iterating to find channel if not found yet
        bytePos += sizeof(struct DataChannel);
    }

    // Evaluate expression after all is done
    actuator_expr_eval();
}

void actuator_expr_eval() {
    // Go through each usedChannel
    uint16_t bytePos = placePosInEEPROM;
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel *channel = (struct DataChannel *) &EEPROM_DATA[bytePos];
        // Map data received from pckCD to expression
        struct expr_var *var = expr_var(&channel->name, 1);
        var->value = vars[i];
        // Keep iterating to map everything else
        bytePos += sizeof(struct DataChannel);
    }
    // Call evaluate
    expr_eval(expression);

}

//TODO:
// move all module specific hardware to sensor.h
// way to set multiple output values in algorithm
// add ability to configure what sensor input goes to what dataCh(usb)
// add version number and crc?
// add configuration update ability
