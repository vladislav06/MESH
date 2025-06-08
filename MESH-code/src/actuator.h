//
// Created by vm on 25.21.5.
//

#ifndef MESH_CODE_ACTUATOR_H
#define MESH_CODE_ACTUATOR_H
#include "protocol.h"

extern uint8_t placePosInEEPROM;
extern struct expr* expression;

/*
 * Public functions for the ld2410b sensor.
 */
void actuator_handle_CD(struct PacketCD * pck);

#endif //MESH_CODE_ACTUATOR_H
