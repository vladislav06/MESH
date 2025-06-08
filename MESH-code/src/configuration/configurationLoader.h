//
// Created by vm on 25.7.6.
//

#ifndef MESH_CODE_CONFIGURATIONLOADER_H
#define MESH_CODE_CONFIGURATIONLOADER_H


#include <stdint.h>

extern uint8_t *rxBuf;
extern uint32_t rxLen;


// Will try load configuration thru usb cdc
void loadConfigurationThruUSB();

void startLoadConfiguration();
// returns true if rxBuf has new data
bool newDataIsAvailable();
// must be called after new data was processed
void dataWasReceived();

#endif //MESH_CODE_CONFIGURATIONLOADER_H
