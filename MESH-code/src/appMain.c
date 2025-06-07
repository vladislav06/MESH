//
// Created by vm on 25.6.4.
//
#include <math.h>
#include <sys/unistd.h>
#include <stdio.h>
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "hw.h"
//#include "expr.h"
#include <string.h>

#include "ld2410b.h"
#define D3 GPIO_PIN_15
#define D4 GPIO_PIN_1

static struct cc1101 *cc;
static int n = 0;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2) {

    HAL_Delay(2000);
    printf("%#08lX\r\n", HAL_GetUIDw2());
    //    //init utilities
    hw_enable_ld(true);
    utils_init(htim6,huart2);
//

//    struct mallinfo mi = mallinfo();
//    printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
//    printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
//    printf("Total free space (fordblks):           %d\n", mi.fordblks);
//
////    expr_test();
//    char *s = "x*2**x**2**x**2**x";
//    struct expr_var_list vars = {0};
//    static struct expr_func user_funcs[] = {
//            {NULL, NULL, NULL, 0},
//    };
//    struct expr *e = expr_create(s, strlen(s), &vars, user_funcs);
//
//    struct expr_var *varx = expr_var(&vars, "x", 1);
//    varx->value = 1;
//    if (e == NULL) {
//        printf("FAIL: %s returned NULL\n", s);
//        return;
//    }
//    float result = expr_eval(e);
//    printf("result = %f\n", result);
//
//    expr_destroy(e, &vars);
//
//    mi = mallinfo();
//    printf("Total non-mmapped bytes (arena):       %d\n", mi.arena);
//    printf("Total allocated space (uordblks):      %d\n", mi.uordblks);
//    printf("Total free space (fordblks):           %d\n", mi.fordblks);


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

    /// LD2410B Code
    struct ld2410b ld;
    ld2410b_create(&ld, huart2);
    ld2410b_debugOn(&ld);
    ld2410b_configMode(&ld,true);
    while (true) {
        //ld2410b_configMode(&ld,true);
        //HAL_Delay(1000);

        //ld2410b_configMode(&ld,false);
        //HAL_Delay(1000);

        // ld2410b_toggleRangeRes(&ld);
        // HAL_Delay(1000);

        //HAL_Delay(15000);
        ld2410b_startBottomNoiseDetection(&ld);

        // ld2410b_processReport(&ld);
        // HAL_Delay(1000);
    }



    // cc1101 initialisation
    cc = cc1101_create(GPIO_PIN_4, GPIO_PIN_14, GPIO_PIN_15, hspi1);

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
    enum CCStatus status = cc1101_begin(cc, config);
    if (status == STATUS_CHIP_NOT_FOUND) {
        printf("STATUS_CHIP_NOT_FOUND\r\n");
    }

    //register EXTI callback
    struct Interrupt intr0 = {
            .interrupt=cc1101_exti_callback_gd0,
            .arg=cc,
            .gpio=GPIO_PIN_14,
    };
    struct Interrupt intr1 = {
            .interrupt=cc1101_exti_callback_gd2,
            .arg=cc,
            .gpio=GPIO_PIN_15,
    };
    register_interrupt(intr0, 0);
    register_interrupt(intr1, 1);

    cc->trState = RX_STOP;
    enableInterrupts = 1;

    uint32_t uid = HAL_GetUIDw2();

    switch (uid) {
        case 0x3F004D:
            receiver();
            break;
        case 0x3C004C:
        case 0X280050:
            break;
    }
    transmitter();
}


void transmitter(void) {
    cc->defaultState = DEF_RX;
    cc1101_receiveCallback(cc, on_receive_empty);
    cc1101_start_receive(cc);
    struct Packet packet;
    packet.id = HAL_GetUIDw2();
    while (1) {
        HAL_GPIO_WritePin(GPIOA, D3, 1);
        packet.rssi = cc1101_rssiToDbm(cc1101_getRssi(cc));
        packet.checksum = packet.id ^ *((uint32_t *) &packet.rssi);
        cc1101_transmit_sync(cc, (uint8_t *) &packet, sizeof(struct Packet), 0);
        HAL_GPIO_WritePin(GPIOA, D3, 0);
        HAL_Delay(200);
    }
}

void receiver(void) {
    cc->defaultState = DEF_RX;
    cc1101_receiveCallback(cc, on_receive);
    cc1101_start_receive(cc);
    while (1) {
        HAL_Delay(100);
    }
}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    HAL_GPIO_WritePin(GPIOA, D4, 1);
    struct Packet packet = *((struct Packet *) data);
    if (packet.checksum == (packet.id ^ *((uint32_t *) &packet.rssi))) {
        float recRssi = cc1101_rssiToDbm(rssi);
        int recrssidbni = floor(recRssi);
        int recrssidbnf = floor((recRssi - recrssidbni) * 10);
        printf("Received rssi: %3d.%1d | ", recrssidbni, recrssidbnf);
        float prssi = packet.rssi;
        int rssidbni = floor(prssi);
        int rssidbnf = floor((prssi - rssidbni) * 10);
        printf("id: %#08lX, rssi:%3d.%1d", packet.id, rssidbni, rssidbnf);
        printf("\r\n");
    }
    //transmit answer
    HAL_GPIO_WritePin(GPIOA, D4, 0);
}

void on_receive_empty(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    UNUSED(data);
    UNUSED(len);
    UNUSED(rssi);
    UNUSED(lq);
}
