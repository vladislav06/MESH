#include "ld2410b.h"

#include <malloc.h>
#include <string.h>

#include "stdint.h"
#include "stdio.h"

#include "utils.h"

//head size == tail size
#define ld2410ConfigSize 4
#define maxResponseSize (ld2410ConfigSize * 2 + 30)
#define maxCommandSize (ld2410ConfigSize * 2 + 22)

/// Packet constants:
// Head and tail fixed parts
const uint8_t ld2410HeadConfig[4] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t ld2410HeadReport[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t ld2410TailConfig[4] = {4, 3, 2, 1};
const uint8_t ld2410TailReport[4] = {0xF8, 0xF7, 0xF6, 0xF5};

// Commands - first number always represents the size of the command, minus 2
const uint8_t configEnable[6] = {4, 0, 0xFF, 0, 1, 0};
const uint8_t configDisable[4] = {2, 0, 0xFE, 0};
static uint8_t toggleRangeRes[6] = {4, 0, 0xAA, 0, 1, 0}; //not const to save on memory
const uint8_t startBottomNoiseDetection[6] = {4, 0, 0xBB, 0, 0x0A, 0};

/// Utilities
// Array for response - head, 30 max body, tail
static uint8_t response[maxResponseSize] = {0};
// Array for command - head, 22 max body, tail
static uint8_t commandToSend[maxCommandSize] = {0};

/// Private functions
void writeCommand(uint8_t *destination, const uint8_t *commandToWrite, const uint8_t size, const uint8_t startingPoint) {
    memcpy(&destination[startingPoint], commandToWrite, size);
}

// TODO: What if the response is flooded? Instead find the first symbol of the end and start comparing.
bool responseEndsWith(const struct ld2410b *instance, const uint8_t *endsWith, uint8_t endsWithLength) {
    // Find the end of the command first
    uint8_t endPosition = maxResponseSize-1;

    while (response[endPosition] == 0) {
        if (endPosition == 0) {
            return false;
        }

        endPosition--;
    }

    // Point to beginning of end
    endPosition -= endsWithLength;
    endPosition += 1;

    // Compare response with what was given
    for (int i = 0; i < endsWithLength; i++) {
        // Example - response is 16 in length, last el pos is 15.
        // endPosition is 15, given endsWithLength is 4.
        // endsWith is contained in elements 12..15 of the response (12,13,14,15 = 4 el).
        // So we need to start comparing from endPosition-endsWithLength+1 =>
        // 15 - 4 + 1 = 12 in our case.
        if (endsWith[i] != response[endPosition + i]) {
            if (instance->_debug) {
                printf("Failed in comparison\n");
                printf("endsWith was %u, response was %u, i was %u\n", endsWith[i], response[i], i);
                printf("\n");
            }
            return false;
        }
    }

    // Check was passed, return true
    return true;
}

uint8_t findResponseBeginning(const struct ld2410b *instance, const uint8_t *startsWith, uint8_t startsWithLength) {
    const uint8_t hasToBeCountedCorrect = startsWithLength - 1;
    const uint8_t beginOffset = 1;

    uint8_t responseBeginning = 0xFF;
    uint8_t countedCorrect = 0;

    bool beginningEndCrossed = false;

    // We are counting through the last startsWithLength-1 elements (e.g. 1, 2, 3 if length == 4) of the beginning,
    // simply because the first one might be gone.

    // Find start of beginning
    for (int i = 0; i < maxResponseSize; i++) {
        if (response[i] == startsWith[beginOffset]) {
            responseBeginning = i;
            countedCorrect++;
            break;
        }
    }

    // Check beginning
    for (int i = responseBeginning; i < maxResponseSize; i++) {
        if (response[i] == startsWith[i+beginOffset]) {
            countedCorrect++;

            // If the element is the last one in the beginning...
            if (response[i] == startsWith[startsWithLength - 1]) {
                beginningEndCrossed = true;
                break;
            }
        }
    }

    if (countedCorrect != hasToBeCountedCorrect || beginningEndCrossed == false) {
        return 0xFF;
    }

    return responseBeginning;
}

bool processAck(struct ld2410b *instance) {
    if (instance->_debug) {
        printf("Received response:\n");
        for (int i = 0; i < maxResponseSize; i++) {
            printf("%02x ", response[i]);
        }
        printf("\n");
    }

    // If the response does not end with a tail, something went wrong
    if (responseEndsWith(instance, ld2410TailConfig, ld2410ConfigSize) == false) {
        if (instance->_debug) {
            printf("responseEndsWith in processAck failed\n");
        }
        return false;
    }

    uint8_t responseBeginning = findResponseBeginning(instance, ld2410HeadConfig, ld2410ConfigSize);
    if (instance->_debug) {
        printf("responseBeginning position is: %d\n", responseBeginning);
    }
    if (responseBeginning == 0xFF) {
        return false;
    }

    // Add one more to point to the beginning of the response
    responseBeginning++;

    // In the main portion of the response, the structure is:
    // 1. two bytes representing the length of the response
    // 2. two bytes representing the command and status:
    // for enabling config, it will be 0xFF 0x01 if successful,
    // 3. general data about the response.

    // Checking whether we are given the length of the response -
    // stored in the first 2 bytes of the response after head
    const uint16_t responseLength = response[responseBeginning] | response[responseBeginning + 1] << 8;

    if (responseLength <= 0) {
        return false;
    }

    // Read the type of the command - two bytes after the beginning
    const uint16_t command = response[responseBeginning + 2] | response[responseBeginning + 3] << 8;

    if (instance->_debug) {
        printf("Command in processAck read as: %u\n", command);
    }

    switch (command) {
        case 0x1FF: // entered config mode
            instance->isConfig = true;
            // read protocol version
            instance->version = response[responseBeginning + 4] | (response[responseBeginning + 5] << 8);
            break;

        case 0x1FE: // exited config mode
            instance->isConfig = false;
            break;

        case 0x1AA: // toggle range resolution
            instance->rangeResIsMax = instance->rangeResIsMax == true ? false : true;
            break;

        // TODO: Not done yet. Complete all of the code related to this cmd.
        case 0x10B: // bottom noise detection completed
            instance->timeout = 100; // reset timeout
            break;

        // Invalid command type, return false
        default:
            return false;
    }

    return true;
}

bool processReport(struct ld2410b *instance) {
    if (instance->_debug) {
        printf("Received report:\n");
        for (int i = 0; i < maxResponseSize; i++) {
            printf("%02x ", response[i]);
        }
        printf("\n");
    }

    // If the report does not end with a tail, something went wrong
    if (responseEndsWith(instance, ld2410TailReport, ld2410ConfigSize) == false) {
        if (instance->_debug) {
            printf("responseEndsWith in processReport failed\n");
        }
        return false;
    }

    uint8_t responseBeginning = findResponseBeginning(instance, ld2410HeadConfig, ld2410ConfigSize);
    if (instance->_debug) {
        printf("responseBeginning position is: %d\n", responseBeginning);
    }
    if (responseBeginning == 0xFF) {
        return false;
    }

    // Add one more to point to the beginning of the report (where the length starts)
    responseBeginning++;

    // After beginning of response, per docs:
    // 2 bytes for length (+0 | +1 << 8), 1 for report type (+2), 1 for head (+3, 0xAA default).

    // Check that report type is 2 - "Target Basic Information"
    if (response[responseBeginning + 2] != 2) {
        return false;
    }

    // Skip over head (1 byte...)

    // Record received data in order of its retrieval
    instance->target_state = response[responseBeginning+4];

    instance->target_distanceMovement = response[responseBeginning+5] | response[responseBeginning+6] << 8;
    instance->target_energyValueMovement = response[responseBeginning+7];

    instance->target_distanceStationary = response[responseBeginning+8] | response[responseBeginning+9] << 8;
    instance->target_energyValueStationary = response[responseBeginning+10];

    instance->detectionDistance = response[responseBeginning+11] | response[responseBeginning+12] << 8;

    return true;
}

/// Public functions
struct ld2410b *ld2410b_create(struct ld2410b *instance, UART_HandleTypeDef *uart) {
    instance->timeout = 500;

    instance->rangeResIsMax = true;

    instance->_debug = false;
    instance->isConfig = false;

    instance->version = 0;

    instance->uart = uart;

    return instance;
}

void ld2410b_debugOn(struct ld2410b *instance) {
    instance->_debug = true;
}

void ld2410b_debugOff(struct ld2410b *instance) {
    instance->_debug = false;
}

bool ld2410b_configMode(struct ld2410b *instance, const uint8_t enable) {
    if (enable == true /*&& instance->isConfig == false*/)
        return ld2410b_sendCommand(instance, configEnable);
    if (enable == false && instance->isConfig == true)
        return ld2410b_sendCommand(instance, configDisable);

    return false;
}

bool ld2410b_toggleRangeRes(struct ld2410b *instance) {
    // Modify 5th element representing the value of the command
    // 0 - set to 0.2m, 1 - set to 0.75m (default)
    toggleRangeRes[4] = instance->rangeResIsMax == true ? 0 : 1;

    return ld2410b_sendCommand(instance, toggleRangeRes);
}

// TODO: Not done yet. Complete all of the code related to this cmd.
bool ld2410b_startBottomNoiseDetection(struct ld2410b *instance) {
    // Increase timeout for correct retrieval (10s+2s for redundancy)
    instance->timeout = 12000;

    return ld2410b_sendCommand(instance, startBottomNoiseDetection);
}

bool ld2410b_sendCommand(struct ld2410b *instance, const uint8_t *command) {
    if (instance->_debug == true) {
        printf("Command received in ld2410b_sendCommand:\n");
        for (int i = 0; i < command[0]+2; i++) {
            printf("%02x ", command[i]);
        }
        printf("\n");
    }

    // First value always holds the size, minus 2
    uint8_t commandSize = command[0] + 2;
    uint8_t totalSize = ld2410ConfigSize * 2 + commandSize;

    // Write the command
    writeCommand(commandToSend, ld2410HeadConfig, ld2410ConfigSize,
                 0);
    writeCommand(commandToSend, command, commandSize,
                 ld2410ConfigSize);
    writeCommand(commandToSend, ld2410TailConfig, ld2410ConfigSize,
                 ld2410ConfigSize + commandSize);

    memset(response, 0, maxResponseSize);

    // TODO: temporary testing workaround. Remove later.
    uint8_t testCommand[14] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};

    // TODO: Oscilloscope testing proved that commands with incorrect size are sent.
    // size was initially totalSize, try to come back to it. Testing can be set as 14.
    //HAL_UART_Transmit(instance->uart, commandToSend, 14, 1000);
    // TODO: temporary testing workaround. Remove later.
    HAL_UART_Transmit(instance->uart, testCommand, 14, 2000);
    // TODO: Incorrect size of 20 can be set for testing, but use maxResponseSize for prod.
    HAL_UART_Receive(instance->uart, response, maxResponseSize, instance->timeout);

    if (instance->_debug == true) {
        printf("Built command to send:\n");
        for (int i = 0; i < maxCommandSize; i++) {
            printf("%02x ", commandToSend[i]);
        }
        printf("\n");
        printf("Built command to send total size: %u\n", totalSize);
    }

    memset(commandToSend, 0, maxCommandSize);

    return processAck(instance);
}

// TODO: Bugs out, fix it.
bool ld2410b_processReport(struct ld2410b *instance) {
    memset(response, 0, maxResponseSize);

    HAL_UART_Receive(instance->uart, response, maxResponseSize, instance->timeout);

    return processReport(instance);
}
