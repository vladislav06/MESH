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

static struct cc1101 cc;
static int n = 0;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2) {

    HAL_Delay(2000);
    printf("%#08lX\r\n", HAL_GetUIDw2());
    // init utilities
    hw_enable_ld(true);
    utils_init(htim6);

//

    struct mallinfo mi = mallinfo();
    printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
    printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
    printf("Total free space (fordblks):           %d\n", mi.fordblks);

//    expr_test();
    char *s = "x*2**x**2**x**2**x";
//    struct expr_var_list vars = {0};
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
    printf("result = %f\n", result);

//    expr_destroy(e, &vars);

    mi = mallinfo();
    printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
    printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
    printf("Total free space (fordblks):           %d\n", mi.fordblks);


//
//    /*
//    * stm32 has 96 bit large unique id which consists of
//    * UID[31:0]:  X and Y coordinates on the wafer expressed in BCD format
//    * UID[39:32]: WAF_NUM[7:0] Wafer number (8-bit unsigned number)
//    * UID[63:40]: LOT_NUM[23:0] Lot number (ASCII encoded)
//    * UID[95:64]: LOT_NUM[55:24] Lot number (ASCII encoded)
//    * 96 bits are equally split among 3 uint32
//    * Because of that and the fact that all 10 chips are from same batch, first 2 uint32 are the same,
//    * and does not require check
//    * example:
//    * 0  256324400
//    * 1  909523256
//    * 2  4128845
//    * 0  256324400
//    * 1  909523256
//    * 2  4128848
//    */
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());
//
//    uint8_t data_buffer[500] = {0};
//////    HAL_UART_Receive_DMA(huart2, data_buffer, 1000);
//
//    // transmitting 0 fixed bug
//    HAL_UART_Transmit(huart2, data_buffer, 1, 100);
////
//    uint8_t data_start[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
////    uint8_t data_config[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x62, 0x00,  0x04, 0x03, 0x02, 0x01};
//    HAL_UART_Transmit(huart2, data_start, 14, 100);
//////    HAL_UART_Transmit(huart2, data_start, 14, 5000);
////
//////    HAL_UART_Transmit(huart2, data_config, 12, 5000);
//////    uint8_t data_end[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
//////    HAL_UART_Transmit(huart2, data_end, 12, 5000);
//////    HAL_UART_Transmit(huart2, data_end, 12, 100);
//    HAL_UART_Receive(huart2, data_buffer, 20, 100);
//    HAL_Delay(5000);
//    for (int i = 0; i < 500; i++) {
//        printf("%x ", data_buffer[i]);
//    }
//    printf("\t\n");
////    printf("s1:%d|s2:%d",s1);

//    struct PacketARR packetArr;
//    packetArr.header.



    // cc1101 initialisation
    cc1101_create(&cc, GPIO_PIN_4, GPIO_PIN_14, GPIO_PIN_15, hspi1);

    // config cc1101
    struct CCconfig config = {
            .mod=MOD_GFSK,
            .freq=433.8,//Mhz
            .drate=1.2,//kbaud/s
            .power=10,
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
//
//    uint32_t uid = HAL_GetUIDw2();
//
//    switch (uid) {
//        case 0x3F004D:
//            receiver();
//            break;
//        case 0x3C004C:
//        case 0X280050:
//            break;
//    }
//    transmitter();


    wireless_comms_init(&cc);

    cc1101_receiveCallback(&cc, on_receive);

    uint32_t counter = 0;
    // main loop, runs at 10Hz
    while (true) {
        uint32_t start = HAL_GetTick();
        // each 100ms
        if (HAL_GetTick() % 100 == 0) {

        }
        // each 1000ms / 1s
        if (HAL_GetTick() % 10 == 0) {
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
            cc1101_transmit_sync(&cc, (uint8_t *) &packetNRR, sizeof(struct PacketNRR), 0);
        }
        // each 5000ms / 5s
        if (counter % 50 == 0) {
            printf("routing table:\n");
            for (int i = 0; i < NEIGHBOUR_TABLE_SIZE; i++) {
                printf("neighbourId: %d\n                   \n", neighbourTable[i].neighbourId);
                for (int i = 0; i < DESTINATION_COUNT; i++) {
                    printf(" %d |", neighbourTable[i].destinations[i].destinationId);
                }
                printf("\n");
            }
        }
        counter++;
        uint32_t end = HAL_GetTick();
        if (end - start <100) {
            HAL_Delay(100 - (end - start));
        }
    }
}
