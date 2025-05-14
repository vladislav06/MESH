#ifndef LD2410B_H
#define LD2410B_H

#include "stdint.h"
#include "utils.h"

#define LD2410_LATEST_FIRMWARE "2.44"

/*
 * enums for certain values - light, output...
 */
enum LightControl {
    LIGHT_NOT_SET = -1,
    LIGHT_NO_LIGHT_CONTROL,
    LIGHT_BELOW_THRESHOLD,
    LIGHT_ABOVE_THRESHOLD
};

enum OutputControl {
    OUTPUT_NOT_SET = -1,
    OUTPUT_DEFAULT_LOW,
    OUTPUT_DEFAULT_HIGH,
};

enum AutoStatus {
    AUTO_NOT_SET = -1,
    AUTO_NOT_IN_PROGRESS,
    AUTO_IN_PROGRESS,
    AUTO_COMPLETED
};

/*
 * Structure for ld2410b.
 */
struct ld2410b {
    uint32_t timeout;

    uint8_t maxRange;
    uint8_t noOne_window;

    uint8_t lightThreshold;

    enum LightControl lightControl;
    enum OutputControl outputControl;
    enum AutoStatus autoStatus;

    uint8_t _debug;
    uint8_t isEnhanced;
    uint8_t isConfig;

    uint32_t version;
    uint8_t MAC[6];

    int fineRes;

    UART_HandleTypeDef *uart;
};

/*
 * Public functions for the ld2410b sensor.
 */
struct ld2410b* ld2410b_create(UART_HandleTypeDef *uart);

uint8_t ld2410b_configMode(struct ld2410b *instance, uint8_t enable);

uint8_t ld2410b_sendCommand(struct ld2410b *instance, const uint8_t *command);

void ld2410b_debugOn(struct ld2410b *instance);

void ld2410b_debugOff(struct ld2410b *instance);

bool ld2410b_processACK(struct ld2410b *instance);

#endif LD2410B_H
