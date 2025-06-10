//
// Created by vm on 25.10.6.
//

#ifndef MESH_CODE_ACTUATORIMPL_H
#define MESH_CODE_ACTUATORIMPL_H

#include "stdint.h"

void actuator_impl_init();

void actuator_process(uint8_t ch, uint16_t data);

#endif //MESH_CODE_ACTUATORIMPL_H
