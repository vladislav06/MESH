//
// Created by vm on 25.21.5.
//

#ifndef MESH_CODE_WIRELESSCOMS_H
#define MESH_CODE_WIRELESSCOMS_H

#include "cc1101.h"

#include "stdint.h"
#include "protocol.h"

#define DEBUG

#include "utils.h"

#define HANDLE(x, p) case x: \
                        responseLen = handle_##x(p); \
                     break


extern uint16_t updater_id;
extern uint16_t update_process;
extern uint16_t update_length;

void wireless_comms_init(struct cc1101 *cc);

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq);

void try_discover(uint16_t target, uint8_t place, uint8_t sensorCh);

/// Will send CSR to all suitable nodes in routing table
void try_subscribe(uint8_t place, uint8_t sensorCh, uint8_t dataCh);


uint8_t handle_NONE(struct Packet *pck);

// Neighbour Resolution Request
uint8_t handle_NRR(struct Packet *pck);

// Neighbour Resolution Response
uint8_t handle_NRP(struct Packet *pck);

// Channel subscription Request
uint8_t handle_CSR(struct Packet *pck);

// Chanel Data
uint8_t handle_CD(struct Packet *pck);

// Configuration Version
uint8_t handle_CV(struct Packet *pck);

// Configuration Version Request
uint8_t handle_CVR(struct Packet *pck);

// Configuration Part Request
uint8_t handle_CPR(struct Packet *pck);

// Configuration Part ResponSe
uint8_t handle_CPRS(struct Packet *pck);

// Discovery ReQuest
uint8_t handle_DRQ(struct Packet *pck);

// Discovery ResPonse
uint8_t handle_DRP(struct Packet *pck);

// ACK
uint8_t handle_ACK(struct Packet *pck);


#endif //MESH_CODE_WIRELESSCOMS_H
