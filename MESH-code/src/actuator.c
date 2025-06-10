//
// Created by vm on 25.21.5.
//

#include <math.h>
#include "actuator.h"

#define DEBUG

#include "stdio.h"

#include "expr.h"
#include "eeprom.h"
#include "sensor.h"
#include "configuration.h"
#include "wirelessComms.h"

uint16_t configuration_version;
uint16_t configuration_length;
uint16_t placePosInEEPROM;
struct expr *expression;
uint16_t vars[10];
float output[OUTPUT_COUNT];


// forward declr
float expr_if(struct expr_func *f, struct expr_ptr_arr_t *args, void *c);

float expr_out(struct expr_func *f, struct expr_ptr_arr_t *args, void *c);

/// Public functions
void actuator_load_config() {
    // Get current place
    const uint8_t intendedPlace = sensor_place;
    // 0 and 1 hold 0x6996 (garbage-saving purposes), 2 holds the configuration version
    uint16_t bytePos = 2;
    configuration_version = EEPROM_DATA[bytePos] | EEPROM_DATA[bytePos + 1] << 8;

    bytePos += 2;
    const uint8_t placeCount = EEPROM_DATA[bytePos];
    bytePos++;

    // Reset inner variables
    placePosInEEPROM = 0;
    expr_destroy_expr();
    memset(vars, 0, 10);

    // Iterating through the places
    uint8_t skippedPlaceCount = 0;
    while (skippedPlaceCount < placeCount) {
        // Check whether this place is the one we want
        if (EEPROM_DATA[bytePos] == intendedPlace) {
            placePosInEEPROM = bytePos;
        }

        // Skip through usedChannels
        bytePos++;
        uint8_t usedChannelCount = EEPROM_DATA[bytePos];
        bytePos++;
        bytePos += usedChannelCount * sizeof(struct DataChannel);
        // Skip through algorithm and its length
        uint16_t algoLength = EEPROM_DATA[bytePos] | EEPROM_DATA[bytePos + 1] << 8;
        bytePos += 2;
        bytePos += algoLength * sizeof(char);
        bytePos++;
        // Onto the next one
        skippedPlaceCount++;
    }
    configuration_length = bytePos + 1;

    if (placePosInEEPROM == 0) {
        return;
    }

    // Reuse bytePos to inspect the found place - initially points to "place"
    bytePos = placePosInEEPROM;
    bytePos++;
    //skip usedChannelCount and usedChannels
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    // Read channels
    bytePos++;
    bytePos += usedChannelCount * sizeof(struct DataChannel);

    // Read algo - bytePos should already point to algoLength
    const uint16_t algoLength = EEPROM_DATA[bytePos] | EEPROM_DATA[bytePos + 1] << 8;
    bytePos += 2;


//    expression = expr_create(&EEPROM_DATA[bytePos], algoLength, user_funcs);
    bytePos += algoLength;
}

void actuator_subscribe() {
    if (configuration_version == 0 || placePosInEEPROM == 0) {
        return;
    }
    uint16_t bytePos = placePosInEEPROM;

    // Read usedChannelCount
    bytePos++;
    uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    // Read channels
    bytePos++;
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel *channel = (struct DataChannel *) &EEPROM_DATA[bytePos];
        try_discover(0, channel->place, channel->sensorCh);
        // Subscribe to channel based on data provided
        try_subscribe(channel->place, channel->sensorCh, channel->dataCh);
        // Point to next channel
        bytePos += sizeof(struct DataChannel);
    }
}


void actuator_handle_CD(struct PacketCD *pck) {
    LOG("CD was received from:%4x dataCh: %d place: %d from: %d  =  %d\n",
        pck->header.originalSource, pck->dataCh,
        pck->place, pck->sensorCh, pck->value);
    if (configuration_version == 0 || placePosInEEPROM == 0) {
        return;
    }
    // Iterate through places
    uint16_t bytePos = placePosInEEPROM;
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    bytePos++;
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
    if (configuration_version == 0 || placePosInEEPROM == 0) {
        return;
    }
    // Go through each usedChannel
    uint16_t bytePos = placePosInEEPROM;
    bytePos++;
    const uint8_t usedChannelCount = EEPROM_DATA[bytePos];
    bytePos++;
    for (int i = 0; i < usedChannelCount; i++) {
        // Cast bytes to DataChannel
        struct DataChannel *channel = (struct DataChannel *) &EEPROM_DATA[bytePos];
        // Map data received from pckCD to expression
        struct expr_var *var = expr_var(&channel->name, 1);
        var->value = vars[i];
        // Keep iterating to map everything else
        bytePos += sizeof(struct DataChannel);
    }

    // Read algo - bytePos should already point to algoLength
    const uint16_t algoLength = EEPROM_DATA[bytePos] | EEPROM_DATA[bytePos + 1] << 8;
    bytePos += 2;





    static struct expr_func user_funcs[] = {
            {"out", expr_out},
            {"if",  expr_if},
            {"",    nullptr},
    };

    //evaluate each coma separated expression individually
    uint16_t inside = 0;
    uint16_t lastComa = 0;
    for (int i = 0; i < algoLength; i++) {
        if (*(&EEPROM_DATA[bytePos] + i) == '(') {
            inside++;
            continue;
        }
        if (*(&EEPROM_DATA[bytePos] + i) == ')') {
            inside--;
        }
        if (*(&EEPROM_DATA[bytePos] + i) == ',' && inside == 0) {
            //eval
            expr_destroy_expr();
            struct expr *e = expr_create((char *) ((&EEPROM_DATA[bytePos]) + lastComa), i - lastComa, user_funcs);
            if (e == nullptr) {
                printf("Expr cant be compiled\n");
                return;
            }
            expr_eval(e);
            lastComa = i + 1;
        }
        //last expr without coma
        if (i + 2 == algoLength) {
            //eval
            expr_destroy_expr();
            struct expr *e = expr_create((char *) ((&EEPROM_DATA[bytePos]) + lastComa), i - lastComa + 2, user_funcs);
            if (e == nullptr) {
                printf("Expr cant be compiled\n");
                return;
            }
            expr_eval(e);
        }
    }


    for (int i = 0; i < OUTPUT_COUNT; i++) {
        printf("expr out[%d]: %d.%d\n", i, (int) output[i], (int) ((output[i] - (int) output[i]) * 100));
    }
}


float expr_if(struct expr_func *f, struct expr_ptr_arr_t *args, void *c) {
    (void) f, (void) c;
    float statement = expr_eval(&nth_token(*args, 0));
    if (statement > 0.5) {
        return expr_eval(&nth_token(*args, 1));
    } else {
        return expr_eval(&nth_token(*args, 2));
    }
}

float expr_out(struct expr_func *f, struct expr_ptr_arr_t *args, void *c) {
    (void) f, (void) c;
    float outNum = expr_eval(&nth_token(*args, 0));
    if (0 <= outNum && outNum < OUTPUT_COUNT) {
        output[(int) roundf(outNum)] = expr_eval(&nth_token(*args, 1));
    }
    return 0;
}

//TODO:
// move all module specific hardware to sensor.h
// way to set multiple output values in algorithm
// add ability to configure what sensor input goes to what dataCh(usb)
