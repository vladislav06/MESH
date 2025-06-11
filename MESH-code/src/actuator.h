//
// Created by vm on 25.21.5.
//

#ifndef MESH_CODE_ACTUATOR_H
#define MESH_CODE_ACTUATOR_H

#include "protocol.h"

#define OUTPUT_COUNT 10

extern uint16_t configuration_version;
extern uint16_t configuration_length;

extern uint16_t placePosInEEPROM;
extern struct expr *expression;
extern uint16_t vars[10];

extern float output[OUTPUT_COUNT];

/*
 * Public functions for the ld2410b sensor.
 */
/// Load selected configuration, subscribe to required channels and create math expression
void actuator_load_config();

/// Handle new packet, write data and evaluate expression
bool actuator_handle_CD(struct PacketCD *pck);

/// Evaluate expression
void actuator_expr_eval();

void actuator_subscribe();

#endif //MESH_CODE_ACTUATOR_H
