//
// Created by vm on 25.21.5.
//

#include "wirelessComms.h"
#include "protocol.h"
#include "hw.h"
#include "routing.h"
#include "actuator.h"


#include <stdio.h>
#include <memory.h>

#define DEBUG

#include "utils.h"

#define PCK_SIZE(x)  sizeof(struct x) - sizeof(struct Packet)

#define DISCOVERY_REQUEST_COUNT 2
#define REQUESTERS_COUNT 3

struct DiscoveryRequest {
    uint16_t discoveryTarget;
    uint16_t discoveryRequesters[REQUESTERS_COUNT];
};

struct DiscoveryRequest discoveryRequests[DISCOVERY_REQUEST_COUNT] = {0};

//static uint8_t responseBuffer[255];
static struct cc1101 *cc;

void wireless_comms_init(struct cc1101 *cc1101) {
    cc = cc1101;
}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    hw_set_D3(true);
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
        HANDLE(DRQ, pck);
        HANDLE(DRP, pck);
    }
    if (responseLen != 0) {
        cc1101_transmit_sync(cc, cc->txBuf + 1, responseLen, 0);
    }
    hw_set_D3(false);
}

uint8_t handle_NONE(struct Packet *pck) {
    LOG("NONE was received from:%d", pck->sourceId)
    return false;
}

uint8_t handle_NRR(struct Packet *pck) {
    //answer with NRP
    // unaligned write fix!!
    struct PacketNRP *response = (struct PacketNRP *) (cc->txBuf + 1);
    response->header.magic = MAGIC;
    response->header.sourceId = hw_id();
    response->header.destinationId = pck->sourceId;
    response->header.originalSource = hw_id();
    response->header.finalDestination = pck->sourceId;
    response->header.hopCount = 0;
    response->header.packetType = NRP;
    response->header.size = 0;

    printf("NRR received");
    return sizeof(struct PacketNRP);
}

uint8_t handle_NRP(struct Packet *pck) {
    printf("NRP received");
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

        struct PacketCD *response = (struct PacketCD *) (cc->txBuf + 1);
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

uint8_t handle_DRQ(struct Packet *pck) {
    struct PacketDRQ *pckDRQ = (struct PacketDRQ *) pck;
    //if this node is in blacklist, ignore this packet
    if (pckDRQ->blacklisted == hw_id()) {
        return 0;
    }
    //check if this node is a target
    if (pckDRQ->target == hw_id()) {
        struct PacketDRP *response = (struct PacketDRP *) (cc->txBuf + 1);
        response->header.magic = MAGIC;
        response->header.sourceId = hw_id();
        response->header.destinationId = 0;
        response->header.originalSource = hw_id();
        response->header.finalDestination = 0;
        response->header.hopCount = 0;
        response->header.packetType = DRP;
        response->header.size = PCK_SIZE(PacketDRP);
        response->target = pckDRQ->target;
        response->blacklisted = 0;
        return sizeof(struct PacketDRP);
    }

    //check if request was already made
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == pckDRQ->target) {
            //save who asked
            for (int n = 0; n < REQUESTERS_COUNT; n++) {
                if (discoveryRequests[i].discoveryRequesters[n] != 0) {
                    discoveryRequests[i].discoveryRequesters[n] = pckDRQ->header.originalSource;
                    break;
                }
            }
            return 0;
        }
    }

    //check if there is place for new request
    uint8_t free = 0xff;
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == 0) {
            free = i;
            break;
        }
    }

    //no free space
    if (free == 0xff) {
        return 0;
    }

    // create new request
    struct PacketDRQ *request = (struct PacketDRQ *) (cc->txBuf + 1);
    request->header.magic = MAGIC;
    request->header.sourceId = hw_id();
    request->header.destinationId = 0;
    request->header.originalSource = hw_id();
    request->header.finalDestination = pck->finalDestination;
    request->header.hopCount = 0;
    request->header.packetType = DRQ;
    request->header.size = PCK_SIZE(PacketDRQ);
    request->target = pckDRQ->target;
    request->blacklisted = pckDRQ->header.originalSource;

    discoveryRequests[free].discoveryTarget = pckDRQ->target;
    discoveryRequests[free].discoveryRequesters[0] = pckDRQ->header.originalSource;

    return sizeof(struct PacketDRQ);
}

uint8_t handle_DRP(struct Packet *pck) {
    struct PacketDRP *pckDRP = (struct PacketDRP *) pck;

    //check if this node is destination
    if (pckDRP->blacklisted == hw_id()) {
        return 0;
    }

    uint8_t dra = 0xff;
    //answer to all nodes saved in discovery request array
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == pckDRP->target) {
            dra = i;
            struct PacketDRP *response = (struct PacketDRP *) (cc->txBuf + 1);
            response->header.magic = MAGIC;
            response->header.sourceId = hw_id();
            response->header.destinationId = 0;
            response->header.originalSource = hw_id();
            response->header.finalDestination = 0;
            response->header.hopCount = 0;
            response->header.packetType = DRP;
            response->header.size = PCK_SIZE(PacketDRP);
            response->target = pckDRP->target;
            response->blacklisted = pckDRP->header.originalSource;

            memset(&discoveryRequests[dra], 0, sizeof(struct DiscoveryRequest));
            printf("source: %d | target: %d", pckDRP->header.sourceId, pckDRP->target);
            pckDRP->header.originalSource = pckDRP->target;
            routing_processPacket(pck);
            return sizeof(struct PacketDRP);
        }
    }
    return 0;
}

void try_discover(uint16_t target) {
    //add to list to available space or overwrite last request
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == target || discoveryRequests[i].discoveryTarget == 0 ||
            i == DISCOVERY_REQUEST_COUNT - 1) {
            discoveryRequests[i].discoveryTarget = target;
            discoveryRequests[i].discoveryRequesters[0] = 0;
            memset(discoveryRequests[i].discoveryRequesters, 0, sizeof(uint16_t[REQUESTERS_COUNT]));
        }
    }


    //send packet
    struct PacketDRQ packetDRQ = {
            .header.magic = MAGIC,
            .header.sourceId = hw_id(),
            .header.destinationId = 0,
            .header.originalSource = hw_id(),
            .header.finalDestination = 0,
            .header.hopCount = 0,
            .header.packetType = DRQ,
            .header.size = 0,
            .target=target,
    };
    cc1101_transmit_sync(cc, (uint8_t *) &packetDRQ, sizeof(struct PacketDRQ), 0);

}

