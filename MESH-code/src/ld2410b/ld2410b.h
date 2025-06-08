#ifndef LD2410B_H
#define LD2410B_H

#include "stdint.h"
#include "utils.h"

/*
 * enums for certain values - light, output...
 */
enum TargetState {
    NO_TARGET = 0,
    MOVEMENT_TARGET,
    STATIONARY_TARGET,
    MOVEMENT_AND_STATIONARY_TARGET,
    BACKGROUND_NOISE_DETECTION_IN_PROGRESS,
    BACKGROUND_NOISE_DETECTION_SUCCESS,
    BACKGROUND_NOISE_DETECTION_FAIL
};

enum BottomNoiseState {
    BOTTOM_NOISE_DETECTION_NOT_STARTED = 0,
    BOTTOM_NOISE_DETECTION_IN_PROGRESS,
    BOTTOM_NOISE_DETECTION_SUCCESS
};

/*
 * Structure for ld2410b.
 */
struct ld2410b {
    uint32_t timeout;

    // True - 0.75m (default), false - 0.2m
    bool rangeResIsMax;

    enum TargetState target_state;

    uint16_t target_distanceMovement;
    uint8_t target_energyValueMovement;

    uint16_t target_distanceStationary;
    uint8_t target_energyValueStationary;

    uint16_t detectionDistance;

    bool _debug;
    bool isConfig;

    enum BottomNoiseState bottomNoiseState;

    uint32_t version;

    UART_HandleTypeDef *uart;
};

/*
 * Public functions for the ld2410b sensor.
 */
struct ld2410b* ld2410b_create(struct ld2410b *instance, UART_HandleTypeDef *uart);

bool ld2410b_configMode(struct ld2410b *instance, uint8_t enable);

bool ld2410b_toggleRangeRes(struct ld2410b *instance);

bool ld2410b_startBottomNoiseDetection(struct ld2410b *instance);

bool ld2410b_sendCommand(struct ld2410b *instance, const uint8_t *command);

void ld2410b_debugOn(struct ld2410b *instance);

void ld2410b_debugOff(struct ld2410b *instance);

bool ld2410b_processReport(struct ld2410b *instance);

#endif
