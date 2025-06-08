//
// Created by vm on 25.7.6.
//

#include <stddef.h>
#include "configurationLoader.h"
#include "usbd_cdc_if.h"

uint8_t *rxBuf;
uint32_t rxLen;
bool receiveFlag = false;
extern USBD_HandleTypeDef hUsbDeviceFS;

int8_t onReceive(uint8_t *Buf, uint32_t *Len) {
//    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    rxBuf = Buf;
    rxLen = *Len;
    receiveFlag = true;
    return (USBD_OK);
}

bool newDataIsAvailable() {
    return receiveFlag;
}

void dataWasReceived() {
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    receiveFlag = false;
}

void loadConfigurationThruUSB() {
    USBD_Interface_fops_FS.Receive = onReceive;
}
