//
// Created by vm on 25.7.6.
//

#ifndef MESH_CODE_CONFIGURATIONLOADER_H
#define MESH_CODE_CONFIGURATIONLOADER_H


#include <stdint.h>
#include "cc1101.h"

extern uint8_t *rxBuf;
extern uint32_t rxLen;


// Will try load configuration thru usb cdc
void configuration_usb_load_init();

void configuration_usb_start_load();

// returns true if rxBuf has new data
bool configuration_usb_data_available();

// must be called after new data was processed
void configuration_usb_packet_processed();

/// Load new configuration thru usb, if load succeeded returns true, returns false otherwise
bool configuration_wireless_start_load(struct cc1101 *cc);

#endif //MESH_CODE_CONFIGURATIONLOADER_H
