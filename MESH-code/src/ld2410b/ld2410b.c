#include "ld2410b.h"

#include <malloc.h>
#include <string.h>

#include "stdint.h"
#include "stdio.h"

#include "utils.h"

/// Packet constants:
// Head and tail fixed parts
const uint8_t ld2410HeadConfig[4] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t ld2410TailConfig[4] = {4, 3, 2, 1};
const uint8_t ld2410ConfigSize = 4;

// Commands - first number always represents the size of the command, minus 2
const uint8_t configEnable[6] = {4, 0, 0xFF, 0, 1, 0};
const uint8_t configEnableAckSize = 10;
const uint8_t configDisable[4] = {2, 0, 0xFE, 0};
const uint8_t configDisableAckSize = 6;

/// Utilities
// Array for response - head, 30 max body, tail, headSize == tailSize
const uint8_t maxResponseSize = ld2410ConfigSize * 2 + 30;
static uint8_t response[maxResponseSize] = { 0 };

/// Private functions
void writeCommand(uint8_t *destination, const uint8_t *commandToWrite, const uint8_t size, const uint8_t startingPoint) {
    memcpy(&destination[startingPoint], commandToWrite, size);
}

bool responseEndsWith(const uint8_t *response, const uint8_t *endsWith, uint8_t endsWithLength) {
    // Find the end of the command first
    uint8_t endPosition = maxResponseSize;

    while (response[endPosition] != 0) {
        endPosition--;
    }

    // Edge case - if the response is empty
    if (endPosition == 0)
        return false;

    // Compare response with what was given
    for (int i = 0; i < endsWithLength; i--) {
        // Example - response is 16 in length, last el pos is 15.
        // endPosition is 15, given endsWithLength is 4.
        // endsWith is contained in elements 12..15 of the response (12,13,14,15 = 4 el).
        // So we need to start comparing from endPosition-endsWithLength+1 =>
        // 15 - 4 + 1 = 12 in our case.
        if (endsWith[i] != response[endPosition-(endsWithLength+1)+i]) {
            return false;
        }
    }

    // Check was passed, return true
    return true;
}

bool processAck(struct ld2410b *instance, uint8_t *response) {
    if (instance->_debug) {
        printf("Received response:\n");
        printf("%p", response);
        printf("\n");
    }

    // If the response does not end with a tail, something went wrong
    if (responseEndsWith(response, ld2410TailConfig, ld2410ConfigSize) == false) {
        return false;
    }

    // In commands, the structure is:
    // 1. two bytes representing the length of the response
    // 2. two bytes representing the command and status:
    // for enabling config, it will be 0xFF 0x01 if successful,
    // 3. general data about the response.

    // Checking whether we are given the length of the response -
    // stored in the first 2 bytes of the response after head
    const uint16_t responseLength = response[ld2410ConfigSize] | response[ld2410ConfigSize+1] << 8;

    if (responseLength <= 0) {
        return false;
    }

    // Read the type of the command - two bytes after the response length
    const uint16_t command = response[ld2410ConfigSize+2] | response[ld2410ConfigSize+3] << 8;

    switch (command) {
        case 0x1FF: // entered config mode
            instance->isConfig = true;
            instance->version = response[ld2410ConfigSize+4] | (response[ld2410ConfigSize+5] << 8);
        break;

        case 0x1FE: // exited config mode
            instance->isConfig = false;
        break;

        // Invalid command type, return false
        default:
            return false;
    }

    return true;
}

/// Public functions
struct ld2410b* ld2410b_create(UART_HandleTypeDef *uart) {
    struct ld2410b *instance = malloc(sizeof(struct ld2410b));

    instance->timeout = 2000;

    instance->maxRange = 0;
    instance->noOne_window = 0;

    instance->lightThreshold = 0;

    instance->lightControl = LIGHT_NOT_SET;
    instance->outputControl = OUTPUT_NOT_SET;
    instance->autoStatus = AUTO_NOT_SET;

    instance->_debug = false;
    instance->isEnhanced = false;
    instance->isConfig = false;

    instance->version = 0;

    instance->fineRes = -1;

    instance->uart = uart;

    return instance;
}

void ld2410b_debugOn(struct ld2410b *instance) {
    instance->_debug = true;
}

void ld2410b_debugOff(struct ld2410b *instance) {
    instance->_debug = false;
}

uint8_t ld2410b_configMode(struct ld2410b *instance, const uint8_t enable) {
    if (enable == true && instance->isConfig == false)
        return ld2410b_sendCommand(instance, configEnable);
    if (enable == false && instance->isConfig == true)
        return ld2410b_sendCommand(instance, configDisable);

    return false;
}

uint8_t ld2410b_sendCommand(struct ld2410b *instance, const uint8_t *command) {
    if (instance->_debug == true) {
        printf("Sent command:\n");
        printf("%p", command);
        printf("\n");
    }

    // First value always holds the size, minus 2
    uint8_t commandSize = command[0] + 2;
    uint8_t totalSize = ld2410ConfigSize * 2 + commandSize;

    uint8_t *builtCommand = malloc(totalSize * sizeof(uint8_t));

    // Write the command
    writeCommand(builtCommand, ld2410HeadConfig, ld2410ConfigSize,
      0);
    writeCommand(builtCommand, command, commandSize,
      ld2410ConfigSize);
    writeCommand(builtCommand, ld2410TailConfig, ld2410ConfigSize,
      ld2410ConfigSize + commandSize);

    HAL_UART_Transmit(instance->uart, command, totalSize, 100);
    free(builtCommand);

    HAL_UART_Receive(instance->uart, response, maxResponseSize, instance->timeout);
    // TODO: Check whether this delay is needed.
    //HAL_Delay(5000);

    return processAck(instance, response);
}