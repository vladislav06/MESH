//
// Created by vm on 25.6.4.
//
#include <math.h>
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "usbd_cdc_if.h"
#include "hw.h"

#define D3 GPIO_PIN_15
#define D4 GPIO_PIN_1

static struct cc1101 *cc;
static int n = 0;
static TIM_HandleTypeDef *htimc;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             TIM_HandleTypeDef *htim21,
             UART_HandleTypeDef *huart2) {
    //init utilities
    htimc = htim21;
    utils_init(htim6);
    HAL_Delay(1000);

    /*
    * stm32 has 96 bit large unique id which consists of
    * UID[31:0]:  X and Y coordinates on the wafer expressed in BCD format
    * UID[39:32]: WAF_NUM[7:0] Wafer number (8-bit unsigned number)
    * UID[63:40]: LOT_NUM[23:0] Lot number (ASCII encoded)
    * UID[95:64]: LOT_NUM[55:24] Lot number (ASCII encoded)
    * 96 bits are equally split among 3 uint32
    * Because of that and the fact that all 10 chips are from same batch, first 2 uint32 are the same,
    * and does not require check
    * example:
    * 0  256324400
    * 1  909523256
    * 2  4128845
    * 0  256324400
    * 1  909523256
    * 2  4128848
    */
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());
    printf("%#08lX\r\n", HAL_GetUIDw2());

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
    uint8_t data[10] = {0};
    HAL_TIM_Base_Start(htimc);
    while(true){
        int roundTrip = 0;
        for (int i = 0; i < 10; i++) {
            roundTrip += cc1101_measure_round_trip_master(cc, (uint8_t *) data, 1, htimc);
            HAL_Delay(100);
        }
        roundTrip /= 10;
        printf("roundTrip: %u\r\n", roundTrip);
        HAL_Delay(500);
    }

}

void receiver(void) {
    cc->defaultState = DEF_RX;
//    cc1101_receiveCallback(cc, on_receive);
    cc1101_start_receive(cc);
    while (1) {
        cc1101_measure_round_trip_responder(cc);
    }
}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    hw_set_D3(1);
    cc1101_transmit_sync(cc, (uint8_t *) data, 10, 0);
    hw_set_D3(0);
}

void on_receive_empty(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    UNUSED(data);
    UNUSED(len);
    UNUSED(rssi);
    UNUSED(lq);
    HAL_TIM_Base_Stop(htimc);
    printf("CNT: %lu", htimc->Instance->CNT);
    htimc->Instance->CNT = 0;
}
