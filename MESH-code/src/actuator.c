//
// Created by vm on 25.21.5.
//

#include "actuator.h"

#define DEBUG

#include "stdio.h"
#include "utils.h"

void actuator_handle_CD(struct PacketCD *pck) {
    LOG("CD was received from:%d datach: %d place: %d from: %d  =  %d",
        pck->header.originalSource, pck->dataCh,
        pck->place, pck->sensorCh, pck->value)
}
