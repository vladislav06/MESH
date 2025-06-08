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

static struct cc1101 cc;
static int n = 0;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2,
             CRC_HandleTypeDef *hcrc,
             RNG_HandleTypeDef *hrng) {

    HAL_Delay(2000);
    printf("%#08lX\r\n", HAL_GetUIDw2());

    // init utilities
    hw_enable_ld(true);
    utils_init(htim6, huart2, hcrc, hrng);
    loadConfigurationThruUSB();

    char *s = "2+2";

    static struct expr_func user_funcs[] = {
            {"", NULL, NULL, 0},
    };

    struct expr *e = expr_create(s, strlen(s), user_funcs);

    struct expr_var *varx = expr_var("x", 1);
    varx->value = 1;
    if (e == NULL) {
        printf("FAIL: %s returned NULL\n", s);
        return;
    }

    float result = expr_eval(e);
    printf("result = %d\n", (int) result);


    printf("%#X\r\n", hw_id());
    printf("%#X\r\n", hw_id());
    printf("%#X\r\n", hw_id());


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
    if (hw_id() == 0x4f4d) {
        sensor_place = 10;
        sensor_sensorCh = 10;
    }

    // main loop, runs at 10Hz
    while (true)
//    {uint8_t rssi= cc1101_getRssi(&cc);float dbm = cc1101_rssiToDbm(rssi);printf("rssi: %d.%d\n",(int) dbm, (int) (dbm - ((int) dbm)) * 100 );}
    {

        uint32_t start = HAL_GetTick();
        // usb debug each 500ms
        if (HAL_GetTick() % 5 == 0) {
            if (newDataIsAvailable()) {
                for (uint32_t i = 0; i < rxLen; i++) {
                    printf("%d ", rxBuf[i]);
                }
                printf("\n");
                dataWasReceived();
            }
        }
        // each 1000ms / 1s
        if (counter % 10 == 0) {
            if (hw_id() == 0x474d) {
//        if (false) {
                // send discovery packet
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
                cc1101_transmit_sync(&cc, (uint8_t *) &packetNRR, sizeof(struct PacketNRR), 0);
            }
        }
        // try discover then subscribe each 3seconds with 1.5s shift
        if (counter % 30 == 0) {
            if (hw_id() == 0x1d35 ||
                hw_id() == 0x3c4c ||
                hw_id() == 0x2f33) {
                try_discover(0, 10, 10);
            }
        }
        if (counter % 30 == 15) {
            if (hw_id() == 0x1d35 ||
                hw_id() == 0x3c4c ||
                hw_id() == 0x2f33) {
                try_subscribe(10, 10, 1);
            }
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

            memset(neighbourTable, 0, sizeof(struct NeighbourEntry) * NEIGHBOUR_TABLE_SIZE);

        }
        counter++;
        uint32_t end = HAL_GetTick();
        if (end - start < 100) {
            HAL_Delay(100 - (end - start));
        }
    }
}
