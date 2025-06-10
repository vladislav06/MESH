//
// Created by vm on 25.6.4.
//
#define DEBUG

#include <math.h>
#include <sys/unistd.h>
#include <stdio.h>
#include <string.h>
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "hw.h"
#include "protocol.h"
#include "expr.h"
#include "routing.h"
#include "wirelessComms.h"
#include "routing.h"
#include "sensor.h"
#include "configuration/configurationLoader.h"
#include "eeprom.h"
#include "actuator.h"

static struct cc1101 cc;
static int n = 0;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2,
             CRC_HandleTypeDef *hcrc,
             RNG_HandleTypeDef *hrng) {
    hw_init(hadc);
    HAL_Delay(2000);
    printf("%#08lX\r\n", HAL_GetUIDw2());

    // init utilities
    hw_enable_ld(true);
    utils_init(htim6, huart2, hcrc, hrng);
    configuration_usb_load_init();


    // cc1101 initialisation
    cc1101_create(&cc, GPIO_PIN_4, GPIO_PIN_14, GPIO_PIN_15, hspi1);
    // config cc1101
    struct CCconfig config = {
            .mod=MOD_GFSK,
            .freq=433.100,//Mhz
            .drate=10,//kbaud/s
            .power=-20,
            .pktLenMode=PKT_LEN_MODE_VARIABLE,
            .packetMaxLen=255,
            .addrFilterMode=ADDR_FILTER_MODE_NONE,
            .syncMode=SYNC_MODE_16_16,
            .syncWord=0x6996,
            .bandwidth=120,//60Khz
            .crc=false,
            .preambleLen=16,
    };
    enum CCStatus status = cc1101_begin(&cc, config);
    if (status == STATUS_CHIP_NOT_FOUND) {
        printf("STATUS_CHIP_NOT_FOUND\r\n");
        hw_set_D3(true);
        hw_set_D4(true);
        return;
    }

    //register EXTI callback
    struct Interrupt intr0 = {
            .interrupt=cc1101_exti_callback_gd0,
            .arg=&cc,
            .gpio=GPIO_PIN_14,
    };
    struct Interrupt intr1 = {
            .interrupt=cc1101_exti_callback_gd2,
            .arg=&cc,
            .gpio=GPIO_PIN_15,
    };

    register_interrupt(intr0, 0);
    register_interrupt(intr1, 1);

    cc.trState = RX_STOP;
    cc.defaultState = DEF_RX;
    enableInterrupts = 1;

    wireless_comms_init(&cc);

    cc1101_receiveCallback(&cc, on_receive);
    cc1101_start_receive(&cc);

    uint32_t counter = 0;

    switch (hw_id()) {
        case 0X4f4d:
            sensor_place = 2;
            sensor_sensorCh = 0;
            break;
        case 0x3133:
            sensor_place = 2;
            sensor_sensorCh = 3;
            break;
        case 0x3f50:
            sensor_place = 2;
            sensor_sensorCh = 2;
            break;
        case 0x2850:
            sensor_place = 2;
            sensor_sensorCh = 1;
            break;
    }

    if (configuration_usb_data_available()) {
        if (rxLen > 2 && rxBuf[0] == 0x69 && rxBuf[1] == 0x96) {
            configuration_usb_start_load();
            printf("Data was loaded! %2x %2x\n", EEPROM_DATA[0], EEPROM_DATA[1]);
        }
        configuration_usb_packet_processed();
    }
    if (configuration_usb_data_available()) {
        if (rxLen > 2 && rxBuf[0] == 0x69 && rxBuf[1] == 0x96) {
            configuration_usb_start_load();
            printf("Data was loaded! %2x %2x\n", EEPROM_DATA[0], EEPROM_DATA[1]);
        }
        configuration_usb_packet_processed();
    }

    actuator_load_config();

    actuator_subscribe();

    sensor_init(&cc, huart2, hadc);


    // main loop, runs at 10Hz
    while (true) {

        uint32_t start = HAL_GetTick();
        // usb config load every second
        if (counter % 10 == 0) {
            if (configuration_usb_data_available()) {
                if (rxLen > 2 && rxBuf[0] == 0x69 && rxBuf[1] == 0x96) {
                    configuration_usb_start_load();
                    printf("Configuration was loaded, version:%d\n", configuration_version);
                    actuator_load_config();
                    actuator_subscribe();
                }
                configuration_usb_packet_processed();
            }
        }

        // subscribe to datachannels every 5 seconds
        if (counter % 50 == 0) {
            actuator_subscribe();
        }

        //execute algorithm every second
        if (counter % 10 == 0) {
            actuator_expr_eval();
//            struct PacketNRR packetNRR = {
//                    .header.magic = MAGIC,
//                    .header.sourceId = hw_id(),
//                    .header.destinationId = 0,
//                    .header.originalSource = hw_id(),
//                    .header.finalDestination = 0,
//                    .header.hopCount = 0,
//                    .header.packetType = NRR,
//                    .header.size = 0,
//            };
//            calc_crc((struct Packet *) &packetNRR);
//            cc1101_transmit_sync(&cc, (uint8_t *) &packetNRR, sizeof(struct PacketNRR), 0);
        }

        //send sensor data every second
        if (counter % 10 == 5) {
            sensor_send();
        }

        // each 5s ask neighbour for configuration version
        if (counter % 50 == 0) {
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
            cc1101_transmit_sync(&cc, (uint8_t *) &packetCVR, sizeof(struct PacketCVR), 0);
        }

        if (updater_id != 0) {
            // new update is available, initiate update procedure
            configuration_wireless_start_load(&cc);
            printf("Configuration was loaded, version:%d\n", configuration_version);
            actuator_load_config();
            actuator_subscribe();
        }




        // each 10000ms / 10s
        if (counter % 100 == 0) {
//        if (false) {
            printf("routing table:\n");
            for (int i = 0; i < NEIGHBOUR_TABLE_SIZE; i++) {
                printf("neighbourId: %04x        ", neighbourTable[i].neighbourId);
                for (int j = 0; j < DESTINATION_COUNT; j++) {
                    printf(" %04x |", neighbourTable[i].destinations[j].destinationId);
                }
                printf("\n               %02x        ", neighbourTable[i].neighbourPlace);
                for (int j = 0; j < DESTINATION_COUNT; j++) {
                    printf("   %02x |", neighbourTable[i].destinations[j].place);
                }
                printf("\n               %02x        ", neighbourTable[i].neighbourSensorCh);
                for (int j = 0; j < DESTINATION_COUNT; j++) {
                    printf("   %02x |", neighbourTable[i].destinations[j].sensorCh);
                }
                printf("\n");
            }
//            memset(neighbourTable, 0, sizeof(struct NeighbourEntry) * NEIGHBOUR_TABLE_SIZE);
        }

        counter++;
        uint32_t end = HAL_GetTick();
        if (end - start < 100) {
            HAL_Delay(100 - (end - start));
        }
    }
}
