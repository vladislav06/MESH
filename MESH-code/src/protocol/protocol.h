//
// Created by vm on 25.21.5.
//

#ifndef MESH_CODE_PROTOCOL_H
#define MESH_CODE_PROTOCOL_H

#include "stdint.h"
#include "utils.h"

#define MAGIC 0x6996

enum PacketType {
    NONE = 0,
    // Neighbour Resolution Request
    NRR,
    // Neighbour Resolution Response
    NRP,
    // Channel subscription Request
    CSR,
    // Chanel Data
    CD,
    // Configuration Version
    CV,
    // Configuration Version Request
    CVR,
    // Configuration Part Request
    CPR,
    // Configuration Part ResponS
    CPRS,
    // Discovery ReQuest
    DRQ,
    // Discovery ResPonse
    DRP,
};


// Base structure for all packets
struct Packet {
    uint16_t magic;
    // who sends packet
    uint16_t sourceId;
    // to whom this packet belongs directly
    uint16_t destinationId;
    // original source of the packet
    uint16_t originalSource;
    // final destination for the packet
    uint16_t finalDestination;
    uint8_t hopCount;
    enum PacketType packetType;
    uint8_t size;
};

// Neighbour Resolution Request
struct PacketNRR {
    struct Packet header;
};

// Neighbour Resolution Response
struct PacketNRP {
    struct Packet header;
};

// Channel subscription Request
struct PacketCSR {
    struct Packet header;
    uint8_t dataCh;
    uint8_t place;
    uint8_t sensorCh;
};

// Chanel Data
struct PacketCD {
    struct Packet header;
    uint8_t dataCh;
    uint8_t place;
    uint8_t sensorCh;
    uint16_t value;
};

// Configuration Version
struct PacketCV {
    struct Packet header;
    uint16_t hash;
    uint16_t version;
    uint16_t length;
};
// Configuration Version Request
struct PacketCVR {
    struct Packet header;
};

// Configuration Part Request
struct PacketCPR {
    struct Packet header;
    uint16_t start;
};

// Configuration Part ResponS
struct PacketCPRS {
    struct Packet header;
    uint16_t start;
    uint8_t data[224];
};

// Discovery ReQuest
struct PacketDRQ {
    struct Packet header;
    uint16_t target;
    uint16_t blacklisted;
};

// Discovery ResPonse
struct PacketDRP {
    struct Packet header;
    uint16_t target;
    uint16_t blacklisted;
};

bool isPacketValid(struct Packet *packet);


#endif //MESH_CODE_PROTOCOL_H
