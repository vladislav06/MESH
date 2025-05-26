//
// Created by vm on 25.21.5.
//

#ifndef MESH_CODE_ROUTING_H
#define MESH_CODE_ROUTING_H

#include "stdint.h"
#include "protocol.h"

#define NEIGHBOUR_TABLE_SIZE 10
#define DESTINATION_COUNT 5


struct NeighbourEntry {
    uint16_t neighbourId;
    // place == 0 indicates actuator and not a sensor
    uint8_t neighbourPlace;
    // sensorCh == 0 indicates actuator and not a sensor
    uint8_t neighbourSensorCh;

    struct Destination {
        uint16_t destinationId;
        // place == 0 indicates actuator and not a sensor
        uint8_t place;
        // sensorCh == 0 indicates actuator and not a sensor
        uint8_t sensorCh;
    };

    struct Destination destinations[DESTINATION_COUNT];

};

// Device routing/neighbour table. Contains all neighbourhood devices, entries 5 and above are for neighbourId id above id of this controller
// entries 4 and below are for neighbourId id below id of this controller
extern struct NeighbourEntry neighbourTable[NEIGHBOUR_TABLE_SIZE];


// Will try update routing table with this packet
void routing_processPacket(struct Packet *packet);

// Will try update place and sensorCh
void routing_processCDPacket(struct PacketCD *packet);

// Will return destinationId for packet to get to finalDestination
uint16_t routing_getRoute(uint16_t destination);


#endif //MESH_CODE_ROUTING_H
