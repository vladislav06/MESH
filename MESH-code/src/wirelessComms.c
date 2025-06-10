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
#include "sensor.h"
#include "eeprom.h"


#define DISCOVERY_REQUEST_COUNT 2
#define REQUESTERS_COUNT 3

enum DISCOVERY_TYPE {
    DISCOVERY_ID,
    DISCOVERY_ADDRESS,
};

struct DiscoveryRequest {
    uint16_t discoveryTarget;
    uint16_t discoveryRequesters[REQUESTERS_COUNT];
    uint32_t lastRequestTime;
    enum DISCOVERY_TYPE type;
};

struct DiscoveryRequest discoveryRequests[DISCOVERY_REQUEST_COUNT] = {0};

static struct cc1101 *cc;

static uint8_t indexCounter = 0;

uint16_t updater_id = 0;
uint16_t update_process = 0;
uint16_t update_length = 0;

void wireless_comms_init(struct cc1101 *cc1101) {
    cc = cc1101;
}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    hw_set_D3(true);
    struct Packet *pck = (struct Packet *) data;

    float dbm = cc1101_rssiToDbm(rssi);

    //ignore low power packets
    if (dbm < -80) {
        hw_set_D3(false);
        return;
    }

    if (!isPacketValid(pck)) {
        hw_set_D3(false);
        return;
    }

    LOG("Packet received rssi: %d.%d LQ:%d FROM:%04x TYPE:%d\n", (int) dbm, (int) (dbm - ((int) dbm)) * 100, lq,
        pck->sourceId, pck->packetType);

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
        HANDLE(ACK, pck);
    }
    if (responseLen != 0) {
        cc1101_transmit_sync(cc, cc->txBuf + 1, responseLen, 0);
    }
    hw_set_D3(false);
}

uint8_t handle_NONE(struct Packet *pck) {
    LOG("NONE was received from:%d\n", pck->sourceId);
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
    calc_crc((struct Packet *) response);

    return sizeof(struct PacketNRP);
}

uint8_t handle_NRP(struct Packet *pck) {
    routing_processPacket(pck);
    return 0;
}

uint8_t handle_CSR(struct Packet *pck) {
    struct PacketCSR *pckCSR = (struct PacketCSR *) pck;
    if (pckCSR->header.destinationId != hw_id()) {
        return 0;
    }

    if (pckCSR->header.finalDestination == hw_id()) {
        LOG("CSR received for: %d:%d\n", pckCSR->place, pckCSR->sensorCh);
        // check if this node is packets target
        if ((pckCSR->place == sensor_place || pckCSR->place == 0) &&
            (pckCSR->sensorCh == sensor_sensorCh || pckCSR->sensorCh == 0)) {
            routing_processPacket(pck);
            //check if requested data channel exists
            for (int ch = 0; ch < DATA_CHANNEL_COUNT; ch++) {
                if (sensor_dataChannels[ch] == pckCSR->dataCh) {
                    LOG("valid CSR received\n");
                    //try save to subscriber list
                    for (int subs = 0; subs < SUBSCRIBER_COUNT; subs++) {
                        if (subscribers[subs].id == 0 || subscribers[subs].id == pckCSR->header.originalSource) {
                            subscribers[subs].id = pckCSR->header.originalSource;
                            subscribers[subs].dataCh = pckCSR->dataCh;
                            // send ACK OK
                            struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
                            ack->header.originalSource = hw_id();
                            ack->header.sourceId = hw_id();
                            ack->header.finalDestination = pckCSR->header.originalSource;
                            ack->header.destinationId = routing_getRouteById(pckCSR->header.originalSource);
                            ack->header.index = pckCSR->header.index;
                            ack->ackType = ACK_OK;
                            calc_crc((struct Packet *) ack);
                            return sizeof(struct PacketACK);
                        }
                    }
                    // FIXME:no place in subscriber list
                    return 0;
                }
            }
            // no such dataCh
            // send ACK NO_DATA_CH
            struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
            ack->header.originalSource = hw_id();
            ack->header.sourceId = hw_id();
            ack->header.finalDestination = pckCSR->header.originalSource;
            ack->header.destinationId = routing_getRouteById(pckCSR->header.originalSource);
            ack->header.index = pckCSR->header.index;
            ack->ackType = ACK_NO_DATA_CH;
            calc_crc((struct Packet *) ack);
            return sizeof(struct PacketACK);
        }
        return 0;
    }
    // resend

    uint16_t destination = routing_getRouteById(pckCSR->header.finalDestination);
    if (destination == 0) {
        try_discover(pckCSR->header.finalDestination, 0, 0);
        return 0;
    }
    //send CSR
    struct PacketCSR *csr = PCK_INIT(CSR, (cc->txBuf + 1));
    csr->header.originalSource = pckCSR->header.originalSource;
    csr->header.sourceId = hw_id();
    csr->header.destinationId = destination;
    csr->header.finalDestination = pckCSR->header.finalDestination;
    csr->header.index = pckCSR->header.index;
    csr->place = pckCSR->place;
    csr->sensorCh = pckCSR->sensorCh;
    csr->dataCh = pckCSR->sensorCh;
    calc_crc((struct Packet *) csr);
    return sizeof(struct PacketCSR);


//
//    // check if this node is packets target
//    if ((pckCSR->place == sensor_place || pckCSR->place == 0) &&
//        (pckCSR->sensorCh == sensor_sensorCh || pckCSR->sensorCh == 0)) {
//        routing_processPacket(pck);
//        //check if requested data channel exists
//        for (int ch = 0; ch < DATA_CHANNEL_COUNT; ch++) {
//            if (sensor_dataChannels[ch] == pckCSR->dataCh) {
//                LOG("valid CSR received\n");
//                //try save to subscriber list
//                for (int subs = 0; subs < SUBSCRIBER_COUNT; subs++) {
//                    if (subscribers[subs].id == 0 || subscribers[subs].id == pckCSR->header.originalSource) {
//                        subscribers[subs].id = pckCSR->header.originalSource;
//                        subscribers[subs].dataCh = pckCSR->dataCh;
//                        // send ACK OK
//                        struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
//                        ack->header.originalSource = hw_id();
//                        ack->header.sourceId = hw_id();
//                        ack->header.finalDestination = pckCSR->header.originalSource;
//                        ack->header.destinationId = routing_getRouteById(pckCSR->header.originalSource);
//                        ack->header.index = pckCSR->header.index;
//                        ack->ackType = ACK_OK;
//                        calc_crc((struct Packet *) ack);
//                        return sizeof(struct PacketACK);
//                    }
//                }
//                // FIXME:no place in subscriber list
//                return 0;
//            }
//        }
//        // no such dataCh
//        // send ACK NO_DATA_CH
//        struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
//        ack->header.originalSource = hw_id();
//        ack->header.sourceId = hw_id();
//        ack->header.finalDestination = pckCSR->header.originalSource;
//        ack->header.destinationId = routing_getRouteById(pckCSR->header.originalSource);
//        ack->header.index = pckCSR->header.index;
//        ack->ackType = ACK_NO_DATA_CH;
//        calc_crc((struct Packet *) ack);
//        return sizeof(struct PacketACK);
//    }
//    // this node is not a target, resend
//    // lookup in routing table
//    // send to all nodes that match address
//    for (int i = 0; i < NEIGHBOUR_TABLE_SIZE; i++) {
//        if ((neighbourTable[i].neighbourPlace == pckCSR->place || pckCSR->place == 0) &&
//            (neighbourTable[i].neighbourSensorCh == pckCSR->sensorCh || pckCSR->sensorCh == 0) &&
//            neighbourTable[i].neighbourId != 0) {
//            //send CSR
//            struct PacketCSR *csr = PCK_INIT(CSR, (cc->txBuf + 1));
//            csr->header.originalSource = pckCSR->header.originalSource;
//            csr->header.sourceId = hw_id();
//            csr->header.destinationId = neighbourTable[i].neighbourId;
//            csr->header.finalDestination = neighbourTable[i].neighbourId;
//            csr->header.index = pckCSR->header.index;
//            csr->place = pckCSR->place;
//            csr->sensorCh = pckCSR->sensorCh;
//            csr->dataCh = pckCSR->sensorCh;
//            calc_crc((struct Packet *) csr);
//            return sizeof(struct PacketCSR);
//        }
//        for (int n = 0; n < DESTINATION_COUNT; n++) {
//            if ((neighbourTable[i].destinations[n].place == pckCSR->place || pckCSR->place == 0) &&
//                (neighbourTable[i].destinations[n].sensorCh == pckCSR->sensorCh || pckCSR->sensorCh == 0) &&
//                neighbourTable[i].destinations[n].destinationId != 0) {
//                //send CSR
//                struct PacketCSR *csr = PCK_INIT(CSR, (cc->txBuf + 1));
//                csr->header.originalSource = pckCSR->header.originalSource;
//                csr->header.sourceId = hw_id();
//                csr->header.destinationId = neighbourTable[i].neighbourId;
//                csr->header.finalDestination = neighbourTable[i].destinations[n].destinationId;
//                csr->header.index = pckCSR->header.index;
//                csr->place =neighbourTable[i].destinations[n].place;
//                csr->sensorCh = pckCSR->sensorCh;
//                csr->dataCh = pckCSR->sensorCh;
//                calc_crc((struct Packet *) csr);
//                return sizeof(struct PacketCSR);
//            }
//        }
//    }
//    //if not in routing table, try discover and send back ack NO ROUTE
//    try_discover(0, pckCSR->place, pckCSR->sensorCh);
//
//    struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
//    ack->header.originalSource = hw_id();
//    ack->header.sourceId = hw_id();
//    ack->header.finalDestination = pckCSR->header.originalSource;
//    ack->header.destinationId = routing_getRouteById(pckCSR->header.originalSource);
//    ack->header.index = pckCSR->header.index;
//    ack->ackType = ACK_NO_ROUTE;
//    calc_crc((struct Packet *) ack);
//    return sizeof(struct PacketACK);
}

uint8_t handle_CD(struct Packet *pck) {
    struct PacketCD *pckCD = (struct PacketCD *) pck;
    // ignore data packets not for this node
    if (pckCD->header.destinationId != hw_id()) {
        return 0;
    }
    if (pck->finalDestination == hw_id()) {
        // TODO: call incoming chanel data handler
        actuator_handle_CD(pckCD);
    } else {
        // resend
        uint16_t destination = routing_getRouteById(pck->finalDestination);
        if (destination == 0) {
            //FIXME:no destination!!
        }
        struct PacketCD *response = PCK_INIT(CD, cc->txBuf + 1);
        response->header.sourceId = hw_id();
        response->header.destinationId = destination;
        response->header.originalSource = pck->originalSource;
        response->header.finalDestination = pck->finalDestination;
        response->header.hopCount = pck->hopCount - 1;

        response->dataCh = pckCD->dataCh;
        response->place = pckCD->place;
        response->sensorCh = pckCD->sensorCh;
        response->value = pckCD->value;
        calc_crc((struct Packet *) response);

        return sizeof(struct PacketCD);
    }
    return 0;
}

uint8_t handle_CV(struct Packet *pck) {
    struct PacketCV *pckCV = (struct PacketCV *) pck;

    if (pckCV->header.finalDestination != hw_id()) {
        return 0;
    }
    if (pckCV->version > configuration_version) {
        //hang main loop, start update procedure
        updater_id = pckCV->header.originalSource;
        update_length = pckCV->length;
        update_process = 0;
    }
    return 0;
}

uint8_t handle_CVR(struct Packet *pck) {
    struct PacketCVR *pckCVR = (struct PacketCVR *) pck;
//    if (pckCVR->header.finalDestination != hw_id()) {
//        return 0;
//    }
    //respond with configuration version
    struct PacketCV *response = PCK_INIT(CV, cc->txBuf + 1);
    response->header.originalSource = hw_id();
    response->header.sourceId = hw_id();
    response->header.finalDestination = pckCVR->header.originalSource;
    response->header.destinationId = pckCVR->header.originalSource;
    response->version = configuration_version;
    response->length = configuration_length;
    calc_crc((struct Packet *) response);
    return sizeof(struct PacketCV);
}

uint8_t handle_CPR(struct Packet *pck) {
    struct PacketCPR *pckCPR = (struct PacketCPR *) pck;
    if (pckCPR->header.finalDestination != hw_id()) {
        return 0;
    }
    if (pckCPR->start >= EEPROM_SIZE - 225) {
        return 0;
    }
    //respond with part of configuration
    struct PacketCPRS *response = PCK_INIT(CPRS, cc->txBuf + 1);
    response->header.originalSource = hw_id();
    response->header.sourceId = hw_id();
    response->header.finalDestination = pckCPR->header.originalSource;
    response->header.destinationId = pckCPR->header.originalSource;
    response->start = pckCPR->start;
    memcpy(response->data, EEPROM_DATA + pckCPR->start, 224);

    calc_crc((struct Packet *) response);
    return sizeof(struct PacketCPRS);
}

uint8_t handle_CPRS(struct Packet *pck) {
    struct PacketCPRS *pckCPRS = (struct PacketCPRS *) pck;
    if (pckCPRS->header.finalDestination != hw_id()) {
        return 0;
    }
    update_process += 224;
    //flash Configuration
    eeprom_store(pckCPRS->data, 224, pckCPRS->start);
    return 0;
}

uint8_t handle_DRQ(struct Packet *pck) {
    struct PacketDRQ *pckDRQ = (struct PacketDRQ *) pck;
    //if this node is in blacklist, ignore this packet
    if (pckDRQ->blacklisted == hw_id()) {
        return 0;
    }
    //check if this node is a target
    if (pckDRQ->target == hw_id() ||
        ((pckDRQ->place == sensor_place || pckDRQ->place == 0) &&
         (pckDRQ->sensorCh == sensor_sensorCh || pckDRQ->sensorCh == 0))) {
        // respond
        struct PacketDRP response = {
                .header.originalSource = hw_id(),
                .header.sourceId = hw_id(),
                .header.destinationId = pck->sourceId,
                .header.finalDestination = pck->originalSource,
                .header.index = 0,
                .header.magic=MAGIC,
                .header.packetType=DRP,
                .header.size=PCK_SIZE(PacketDRP),
                .sensorCh = sensor_sensorCh,
                .place = sensor_place,
                .blacklisted = 0,
                .target = hw_id(),
        };
        calc_crc((struct Packet *) &response);
        cc1101_transmit_sync(cc, (uint8_t *) &response, sizeof(struct PacketDRP), 0);
    }

    //check if request was already made(and not to old)
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == (pckDRQ->target | (pckDRQ->place << 8 | pckDRQ->sensorCh)) &&
            discoveryRequests[i].type == !pckDRQ->target &&
            (discoveryRequests[i].lastRequestTime + 5000 > HAL_GetTick())) {
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

    //check if there is place for new request, or replace old request
    uint8_t free = 0xff;
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == 0 || discoveryRequests[i].lastRequestTime < HAL_GetTick() - 5000) {
            free = i;
            break;
        }
    }

    //no free space
    if (free == 0xff) {
        return 0;
    }

    // create new request
    struct PacketDRQ *request = PCK_INIT(DRQ, cc->txBuf + 1);
    request->header.sourceId = hw_id();
    request->header.originalSource = hw_id();
    request->header.finalDestination = pck->finalDestination;
    request->target = pckDRQ->target;
    request->blacklisted = pckDRQ->header.originalSource;
    request->place = pckDRQ->place;
    request->sensorCh = pckDRQ->sensorCh;
    calc_crc((struct Packet *) request);

    discoveryRequests[free].discoveryTarget = pckDRQ->target | (pckDRQ->place << 8 | pckDRQ->sensorCh);
    discoveryRequests[free].type = !pckDRQ->target;
    discoveryRequests[free].discoveryRequesters[0] = pckDRQ->header.originalSource;
    discoveryRequests[free].lastRequestTime = HAL_GetTick();

    return sizeof(struct PacketDRQ);
}

uint8_t handle_DRP(struct Packet *pck) {
    struct PacketDRP *pckDRP = (struct PacketDRP *) pck;

    //check if this node is destination
    if (pckDRP->blacklisted == hw_id()) {
        return 0;
    }
    printf("DRP source:%4x, from:%4x\n", pckDRP->target, pckDRP->header.sourceId);

    routing_processPacket(pck);
    routing_processDRPPacket(pckDRP);
    //answer to all nodes saved in discovery request array
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        //find request
        if (discoveryRequests[i].type == !pckDRP->target &&
            (discoveryRequests[i].discoveryTarget == 0 || discoveryRequests[i].discoveryTarget == pckDRP->place << 8 ||
             discoveryRequests[i].discoveryTarget == (pckDRP->target | (pckDRP->place << 8 | pckDRP->sensorCh)))) {

            struct PacketDRP *response = PCK_INIT(DRP, cc->txBuf + 1);
            response->header.sourceId = hw_id();
            response->header.originalSource = pckDRP->header.originalSource;
            response->target = pckDRP->target;
            response->blacklisted = pckDRP->header.sourceId;
            response->place = pckDRP->place;
            response->sensorCh = pckDRP->sensorCh;
            calc_crc((struct Packet *) response);

            memset(&discoveryRequests[i], 0, sizeof(struct DiscoveryRequest));
//            pckDRP->header.originalSource = pckDRP->target;

            return sizeof(struct PacketDRP);
        }
    }
    return 0;
}


uint8_t handle_ACK(struct Packet *pck) {
    struct PacketACK *pckACK = (struct PacketACK *) pck;

    if (pckACK->header.destinationId != hw_id()) {
        return 0;
    }

    if (pckACK->header.finalDestination == hw_id()) {
        LOG("ACK received - from: %04x about:%d\n", pckACK->header.originalSource, pckACK->ackType);
        return 0;
    }

    //resend
    uint16_t route = routing_getRouteById(pckACK->header.finalDestination);
    if (route == 0) {
        try_discover(pckACK->header.finalDestination, 0, 0);
        return 0;
    }
    struct PacketACK *ack = PCK_INIT(ACK, (cc->txBuf + 1));
    ack->header.originalSource = pckACK->header.originalSource;
    ack->header.sourceId = hw_id();
    ack->header.finalDestination = pckACK->header.finalDestination;
    ack->header.destinationId = route;
    ack->header.index = pckACK->header.index;
    ack->ackType = pckACK->ackType;
    calc_crc((struct Packet *) ack);
    return sizeof(struct PacketACK);

}


void try_discover(uint16_t target, uint8_t place, uint8_t sensorCh) {
    //either target or place must be 0
    if (target != 0 && place != 0) {
        hw_set_D4(true);
        LOG("WARN either target or place must be 0!!!");
        hw_set_D4(false);
        return;
    }
    //add to list to available space or overwrite last request
    for (int i = 0; i < DISCOVERY_REQUEST_COUNT; i++) {
        if (discoveryRequests[i].discoveryTarget == (target | (place << 8 | sensorCh)) &&
            discoveryRequests[i].type == !target) {
            //request was already made earlier
            if (discoveryRequests[i].lastRequestTime > HAL_GetTick() - 5000) {
                // do nothing
            } else {
                //request is old, renew
                break;
            }
        }
        if (discoveryRequests[i].discoveryTarget == 0 ||
            i == DISCOVERY_REQUEST_COUNT - 1) {
            // use discoveryTarget as id or as place + sensorCh storage
            discoveryRequests[i].discoveryTarget = target | (place << 8 | sensorCh);
            discoveryRequests[i].type = !target;
            discoveryRequests[i].discoveryRequesters[0] = 0;
            memset(discoveryRequests[i].discoveryRequesters, 0, sizeof(uint16_t[REQUESTERS_COUNT]));
            break;
        }
    }


    //send a packet
    struct PacketDRQ packetDRQ = {
            .header.magic = MAGIC,
            .header.sourceId = hw_id(),
            .header.destinationId = 0,
            .header.originalSource = hw_id(),
            .header.finalDestination = 0,
            .header.hopCount = 0,
            .header.packetType = DRQ,
            .header.size = PCK_SIZE(PacketDRQ),
            .target = target,
            .place = place,
            .sensorCh = sensorCh
    };
    calc_crc((struct Packet *) &packetDRQ);
    cc1101_transmit_sync(cc, (uint8_t *) &packetDRQ, sizeof(struct PacketDRQ), 0);
}

void try_subscribe(uint8_t place, uint8_t sensorCh, uint8_t dataCh) {
    // will send CSR to all suitable nodes in routing table

    for (int i = 0; i < NEIGHBOUR_TABLE_SIZE; i++) {
        if ((neighbourTable[i].neighbourPlace == place || place == 0) &&
            (neighbourTable[i].neighbourSensorCh == sensorCh || sensorCh == 0) &&
            neighbourTable[i].neighbourId != 0) {
            //send CSR
            struct PacketCSR packetCSR = {
                    .header.originalSource = hw_id(),
                    .header.sourceId = hw_id(),
                    .header.destinationId = neighbourTable[i].neighbourId,
                    .header.finalDestination = neighbourTable[i].neighbourId,
                    .header.index = indexCounter,
                    .header.magic=MAGIC,
                    .header.packetType=CSR,
                    .header.size=PCK_SIZE(PacketCSR),
                    .place = neighbourTable[i].neighbourPlace,
                    .sensorCh = neighbourTable[i].neighbourSensorCh,
                    .dataCh = dataCh
            };
            calc_crc((struct Packet *) &packetCSR);
            indexCounter++;
            cc1101_transmit_sync(cc, (uint8_t *) &packetCSR, sizeof(struct PacketCSR), 0);
            HAL_Delay(10);
        }
        for (int n = 0; n < DESTINATION_COUNT; n++) {
            if ((neighbourTable[i].destinations[n].place == place || place == 0) &&
                (neighbourTable[i].destinations[n].sensorCh == sensorCh || sensorCh == 0) &&
                neighbourTable[i].destinations[n].destinationId != 0) {
                //send CSR
                struct PacketCSR packetCSR = {
                        .header.originalSource = hw_id(),
                        .header.sourceId = hw_id(),
                        .header.destinationId = neighbourTable[i].neighbourId,
                        .header.finalDestination = neighbourTable[i].destinations[n].destinationId,
                        .header.index = indexCounter,
                        .header.magic=MAGIC,
                        .header.packetType=CSR,
                        .header.size=PCK_SIZE(PacketCSR),
                        .place = neighbourTable[i].destinations[n].place,
                        .sensorCh = neighbourTable[i].destinations[n].sensorCh,
                        .dataCh = dataCh
                };
                calc_crc((struct Packet *) &packetCSR);
                indexCounter++;
                cc1101_transmit_sync(cc, (uint8_t *) &packetCSR, sizeof(struct PacketCSR), 0);
                HAL_Delay(10);
            }
        }
    }
}
