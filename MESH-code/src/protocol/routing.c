//
// Created by vm on 25.21.5.
//
#include <memory.h>
#include "routing.h"
#include "hw.h"

// Device routing/neighbour table. Contains all neighbourhood devices, entries 5 and above are for neighbourId id above id of this controller
// entries 4 and below are for neighbourId id below id of this controller
struct NeighbourEntry neighbourTable[NEIGHBOUR_TABLE_SIZE] = {0};

void routing_processPacket(struct Packet *packet) {
    // check if packet is from neighbour
    // find neighbour in table
    for (int neighbour = 0; neighbour < NEIGHBOUR_TABLE_SIZE; neighbour++) {
        if (neighbourTable[neighbour].neighbourId == packet->sourceId) {
            //check if destination isn't already saved
            for (int dest = 0; dest < DESTINATION_COUNT; dest++) {
                if (neighbourTable[neighbour].destinations[dest].destinationId == packet->originalSource) {
                    //nothing to do
                    return;
                }
            }

            // move all destinations one position down and insert new destination
            for (int dest = DESTINATION_COUNT - 1; dest > 0; dest--) {
                neighbourTable[neighbour].destinations[dest] = neighbourTable[neighbour].destinations[dest - 1];
            }

            // add packets original source to the table
            neighbourTable[neighbour].destinations[0].destinationId = packet->originalSource;
            neighbourTable[neighbour].destinations[0].place = 0;
            neighbourTable[neighbour].destinations[0].sensorCh = 0;

            return;
        }
    }

    //try to add new neighbour
    //TODO: if table is full, find if sourceId is higher or lower that this device is, then in the according half of the
    // table find if sourceId is less than max id and if is find perfect place so that entries are in id order
    // if table has empty places, still place in perfect spot, but do not delete any entries
    if (packet->sourceId > hw_id()) {
        //search for place
        for (int i = 5; i < NEIGHBOUR_TABLE_SIZE; i++) {
            if (packet->sourceId < neighbourTable[i].neighbourId) {
                //replace
                for (int n = i; n < NEIGHBOUR_TABLE_SIZE - 1; n++) {
                    neighbourTable[n] = neighbourTable[n + 1];
                }
                neighbourTable[i].neighbourId = packet->sourceId;
                neighbourTable[i].neighbourPlace = 0;
                neighbourTable[i].neighbourSensorCh = 0;
                memset(neighbourTable[i].destinations, 0, sizeof(struct Destination[5]));
            }
        }
    } else if (packet->sourceId < hw_id()) {
        //search for place
        for (int i = 4; i >= 0; i++) {
            if (packet->sourceId < neighbourTable[i].neighbourId) {
                //replace
                for (int n = 0; n < i; n++) {
                    neighbourTable[n] = neighbourTable[n + 1];
                }
                neighbourTable[i].neighbourId = packet->sourceId;
                neighbourTable[i].neighbourPlace = 0;
                neighbourTable[i].neighbourSensorCh = 0;
                memset(neighbourTable[i].destinations, 0, sizeof(struct Destination[5]));
            }
        }
    }
}

void routing_processCDPacket(struct PacketCD *packet) {
    // find device
    for (int i = 0; i < NEIGHBOUR_TABLE_SIZE; i++) {
        if (neighbourTable[i].neighbourId == packet->header.sourceId) {
            //insert
            neighbourTable[i].neighbourPlace = packet->place;
            neighbourTable[i].neighbourSensorCh = packet->sensorCh;
        }
        for (int n = 0; n < DESTINATION_COUNT; n++) {
            if (neighbourTable[i].destinations[n].destinationId == packet->header.sourceId) {
                neighbourTable[i].destinations[n].sensorCh = packet->sensorCh;
                neighbourTable[i].destinations[n].place = packet->place;
            }
        }
    }
}

uint16_t routing_getRoute(uint16_t destination) {
    return 0;
}


