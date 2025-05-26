//
// Created by vm on 25.21.5.
//

#include "wirelessComms.h"
#include "protocol.h"
#include "hw.h"
#include "routing.h"
#include "actuator.h"

#define DEBUG

#include <stdio.h>
#include "utils.h"

static uint8_t responseBuffer[255];
static struct cc1101 *cc;

void wireless_comms_init(struct cc1101 *cc1101) {
    cc = cc1101;
}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    LOG("Packet received rssi: %d !", rssi);
    struct Packet *pck = (struct Packet *) data;
    if (!isPacketValid(pck)) {
        LOG("Invalid packet");
    }
    uint8_t responseLen = 0;

    switch (pck->packetType) {
        HANDLE(NONE, pck);
        HANDLE(NRR, pck);
        HANDLE(NRP, pck);
        HANDLE(CSR, pck);
        HANDLE(CD, pck);
        HANDLE(CV, pck);
        HANDLE(CVR, pck);
        HANDLE(CPR, pck);
        HANDLE(CPRS, pck);
    }
    if (responseLen != 0) {
        cc1101_transmit_sync(cc, responseBuffer, responseLen, 0);
    }
}

uint8_t handle_NONE(struct Packet *pck) {
    LOG("NONE was received from:%d", pck->sourceId)
    return false;
}

uint8_t handle_NRR(struct Packet *pck) {
    //answer with NRP
    struct PacketNRP *response = (struct PacketNRP *) responseBuffer;
    response->header.magic = MAGIC;
    response->header.sourceId = hw_id();
    response->header.destinationId = pck->sourceId;
    response->header.originalSource = hw_id();
    response->header.finalDestination = pck->sourceId;
    response->header.hopCount = 0;
    response->header.packetType = NRP;
    response->header.size = 0;
    return sizeof(struct PacketNRP);
}

uint8_t handle_NRP(struct Packet *pck) {
    routing_processPacket(pck);
    return 0;
}

uint8_t handle_CSR(struct Packet *pck) {
    return 0;
}

uint8_t handle_CD(struct Packet *pck) {
    struct PacketCD *pckCD = (struct PacketCD *) pck;
    // ignore data packets not for this node
    if (pck->destinationId != hw_id()) {
        return 0;
    }

    if (pck->finalDestination == hw_id()) {
        // TODO: call incoming chanel data handler
        actuator_handle_CD(pckCD);
    } else {
        // resend
        uint16_t destination = routing_getRoute(pck->finalDestination);

        struct PacketCD *response = (struct PacketCD *) responseBuffer;
        response->header.magic = MAGIC;
        response->header.sourceId = hw_id();
        response->header.destinationId = destination;
        response->header.originalSource = pck->originalSource;
        response->header.finalDestination = pck->finalDestination;
        response->header.hopCount = pck->hopCount - 1;
        response->header.packetType = CD;
        response->header.size = 5;

        response->dataCh = pckCD->dataCh;
        response->place = pckCD->place;
        response->sensorCh = pckCD->sensorCh;
        response->value = pckCD->value;
        return sizeof(struct PacketCD);
    }
    return 0;
}

uint8_t handle_CV(struct Packet *pck) {
    return 0;
}

uint8_t handle_CVR(struct Packet *pck) {
    return 0;
}

uint8_t handle_CPR(struct Packet *pck) {
    return 0;
}

uint8_t handle_CPRS(struct Packet *pck) {
    return 0;
}

