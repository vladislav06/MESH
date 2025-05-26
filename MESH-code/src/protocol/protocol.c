//
// Created by vm on 25.21.5.
//
#include "protocol.h"
#include "utils.h"

bool isPacketValid(struct Packet *packet) {
    if (packet->magic == MAGIC && packet->packetType <= CPRS) {
        return true;
    }
    return false;
}
