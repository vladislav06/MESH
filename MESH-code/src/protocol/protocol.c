//
// Created by vm on 25.21.5.
//
#include <stdio.h>
#include "protocol.h"

#undef DEBUG

#include "utils.h"

bool isPacketValid(struct Packet *packet) {
//    LOG("magic: %d, type: %d, crc: %d\n", packet->magic == MAGIC, packet->packetType < END, crc_good(packet));
    return packet->magic == MAGIC && packet->packetType < END && crc_good(packet);
}

bool crc_good(volatile struct Packet *packet) {
    uint16_t crc = packet->crc;
    packet->crc = 0;
    uint16_t calced_crc = utils_calc_crc((uint8_t *) packet, sizeof(struct Packet) + packet->size);
    packet->crc = crc;
    LOG("crc: %d-%d\n", calced_crc, crc);
    return calced_crc == crc;
}

void calc_crc(struct Packet *packet) {
    packet->crc = 0;
    packet->crc = utils_calc_crc((uint8_t *) packet, sizeof(struct Packet) + packet->size);
}
