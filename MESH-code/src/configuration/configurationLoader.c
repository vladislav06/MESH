//
// Created by vm on 25.7.6.
//

#include <stddef.h>
#include "configurationLoader.h"
#include "usbd_cdc_if.h"
#include "eeprom.h"
#include "wirelessComms.h"
#include "actuator.h"
#include "hw.h"
uint8_t *rxBuf;
uint32_t rxLen;
bool receiveFlag = false;
extern USBD_HandleTypeDef hUsbDeviceFS;

int8_t configuration_usb_on_packet_rx(uint8_t *Buf, uint32_t *Len) {
//    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    rxBuf = Buf;
    rxLen = *Len;
    receiveFlag = true;
    return (USBD_OK);
}

bool configuration_usb_data_available() {
    return receiveFlag;
}

void configuration_usb_packet_processed() {
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    receiveFlag = false;
}

void configuration_usb_load_init() {
    USBD_Interface_fops_FS.Receive = configuration_usb_on_packet_rx;
}


void configuration_usb_start_load() {
    uint16_t counter = 0;
    while (true) {
        if (configuration_usb_data_available()) {
            eeprom_store_large(rxBuf, 64, counter);
            counter += 64;
            configuration_usb_packet_processed();
        } else {
            break;
        }
        HAL_Delay(100);
    }
}

bool configuration_wireless_start_load(struct cc1101 *cc) {
    while (update_process < update_length) {
        uint8_t tryCount = 0;
        while (tryCount < 5) {
            // send request for part of update,
            struct PacketCPR packetCPR = {
                    .header.magic = MAGIC,
                    .header.sourceId = hw_id(),
                    .header.originalSource = hw_id(),
                    .header.destinationId = updater_id,
                    .header.finalDestination = updater_id,
                    .header.hopCount = 0,
                    .header.packetType = CPR,
                    .header.size = 0,
                    .start = update_process,
            };
            calc_crc((struct Packet *) &packetCPR);
            cc1101_transmit_sync(cc, (uint8_t *) &packetCPR, sizeof(struct PacketCPR), 0);
            // wait for answer,
            uint32_t waitStart = HAL_GetTick();
            uint16_t old_update_process = update_process;
            back:
            if (old_update_process == update_process) {
                if (waitStart + 1000 < HAL_GetTick()) {
                    // send another request,
                    tryCount++;
                    continue;
                }
                goto back;
            } else {
                tryCount = 0;
            }
            //part update successful
            if (update_process >= update_length) {
                //all was downloaded
                updater_id = 0;
                return true;
            }
        }
        // if more than 5 in a row, NRP,
        struct PacketNRR packetNRR = {
                .header.magic = MAGIC,
                .header.sourceId = hw_id(),
                .header.destinationId = 0,
                .header.originalSource = hw_id(),
                .header.finalDestination = 0,
                .header.hopCount = 0,
                .header.packetType = NRR,
                .header.size = 0,
        };
        calc_crc((struct Packet *) &packetNRR);
        cc1101_transmit_sync(cc, (uint8_t *) &packetNRR, sizeof(struct PacketNRR), 0);
        HAL_Delay(1000);
        // then ask another neighbour,
        struct PacketCVR packetCVR = {
                .header.magic = MAGIC,
                .header.sourceId = hw_id(),
                .header.destinationId = 0,
                .header.originalSource = hw_id(),
                .header.finalDestination = 0,
                .header.hopCount = 0,
                .header.packetType = CVR,
                .header.size = 0,
        };
        calc_crc((struct Packet *) &packetCVR);
        cc1101_transmit_sync(cc, (uint8_t *) &packetCVR, sizeof(struct PacketCVR), 0);
        HAL_Delay(500);
        // if repeated more than 10 times and no luck abort,
        // exception idk,
    }
}

