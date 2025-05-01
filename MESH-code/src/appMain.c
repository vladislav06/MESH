//
// Created by vm on 25.6.4.
//
#include <math.h>
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "usbd_cdc_if.h"

#define D3 GPIO_PIN_15
#define D4 GPIO_PIN_1

static struct cc1101 *cc;
static int n = 0;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2) {
    //init utilities
    utils_init(htim6);
    HAL_Delay(1000);

    /*
    * stm32 has 96 bit large unique id which consists of
    * UID[31:0]: X and Y coordinates on the wafer expressed in BCD format
    * UID[63:40]: LOT_NUM[23:0] Lot number (ASCII encoded)
    * UID[39:32]: WAF_NUM[7:0] Wafer number (8-bit unsigned number)
    * UID[95:64]: LOT_NUM[55:24] Lot number (ASCII encoded)
    * 96 bits are equally split among 3 uint32
    * Because of that and the fact that all 10 chips are from same batch, first 2 uint32 are the same,
    * and does not require check
    * ilja
    * 0  256324400
    * 1  909523256
    * 2  4128845
    * vlad
    * 0  256324400
    * 1  909523256
    * 2  4128848
    */
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());
    uint32_t id = HAL_GetUIDw2();

    // cc1101 initialisation
    cc = cc1101_create(GPIO_PIN_4, GPIO_PIN_14, GPIO_PIN_15, hspi1);

    // config cc1101
    struct CCconfig config = {
            .mod=MOD_ASK_OOK,
            .freq=433.8,
            .drate=0.6,
            .power=10,
            .pktLenMode=PKT_LEN_MODE_VARIABLE,
            .packetMaxLen=255,
            .addrFilterMode=ADDR_FILTER_MODE_NONE,
            .syncMode=SYNC_MODE_16_16,
            .syncWord=0x6996,
            .bandwidth=60,
            .crc=false,
            .preambleLen=16,
    };
    enum CCStatus status = cc1101_begin(cc, config);
    if (status == STATUS_CHIP_NOT_FOUND) {
        printf("STATUS_CHIP_NOT_FOUND\r\n");
    }

    //register EXTI callback
    interrupts[0].arg = cc;
    interrupts[0].interrupt = cc1101_exti_callback_gd0;
    interrupts[0].port = GPIOC;
    interrupts[0].gpio = GPIO_PIN_14;

    interrupts[1].arg = cc;
    interrupts[1].interrupt = cc1101_exti_callback_gd2;
    interrupts[1].port = GPIOC;
    interrupts[1].gpio = GPIO_PIN_15;

    cc->trState = RX_STOP;
    enableInterrupts = 1;
    // receiver
    uint32_t uid = HAL_GetUIDw2();
//    cc1101_start_receive(cc);
//    while (1){
//        float rssidmb= cc1101_rssiToDbm(cc1101_getRssi(cc));
//        int rssidbni=floor(rssidmb);
//        int rssidbnf=floor((rssidmb-rssidbni)*100);
//        printf("rssi: %d,%d\r\n",rssidbni, rssidbnf);
//        HAL_Delay(100);
//    }
    switch (uid) {
        case 0x3F004D:
            receiver();
            break;
        case 0x3C004C:
        case 0X280050:
            break;
    }
    transmitter();




//
//    int n = 0;
//    int ns=50;
//    uint8_t transmit[256];
//    char zeros[240] = {0};
//    while (1) {
//        HAL_Delay(500);
//        HAL_GPIO_WritePin(GPIOA, D3, 1);
//        for (int i = 0; i < ns; i++) {
//            zeros[i] = '0';
//        }
//        uint8_t size = sprintf(transmit,
//                               "%s %d",
////                               "123 %d",
//                               zeros, n);
//        cc1101_transmit_async(cc, transmit, size + 1, 0);
//        HAL_GPIO_WritePin(GPIOA, D3, 0);
//        HAL_Delay(500);
//        n++;
//        ns++;
//        if(ns>230){
//            ns=0;
//        }
//    }
//    while (1);
    uint8_t data[256] = {0};
    uint8_t rssi;
    uint8_t lq;
//    while (1) {
//        uint8_t len = cc1101_receive_sync(cc, data, 255, &rssi, &lq, 1000);
//        if (len > 0) {
//            printf("received %d:", len);
//            for (int i = 0; i < len; i++) {
//                printf("%c", data[i]);
//            }
//            printf(" rssi: %d, lq: %d", rssi, lq);
//            memset(data, 0, 256);
//            printf("\r\n");
//        }
//        HAL_Delay(100);
//    }
    while (1) {
        HAL_GPIO_WritePin(GPIOA, D3, 1);
        uint8_t transmit[256];
        uint8_t size = sprintf(transmit,
                               "123 %d",
                               456);
        cc1101_transmit_async(cc, transmit, size + 1, 0);
        HAL_GPIO_WritePin(GPIOA, D3, 0);
        HAL_Delay(100);
    }
//    if (uid == 0x280050) {
//        receiver();
//    }
//    // transmitter
//    if (uid == 0x3F004D) {
//        transmitter();
//    }


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

void HAL_GPIO_EXTI_Callback(uint16_t gpio) {
    if (!enableInterrupts) {
        return;
    }
    for (uint32_t i = 0; i < (sizeof(interrupts) / sizeof(interrupts[0])); i++) {
        if (interrupts[i].gpio == gpio) {
            interrupts[i].interrupt(gpio, interrupts[i].arg);
        }
    }
    //clear all pending interrupts
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_15);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_14);
}
