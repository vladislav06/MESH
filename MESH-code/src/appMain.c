//
// Created by vm on 25.6.4.
//
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "usbd_cdc_if.h"


static struct cc1101 *cc;

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2) {
    //init utilities
    utils_init(htim6);
    HAL_Delay(1000);

    // cc1101 initialisation
    cc = cc1101_create(GPIO_PIN_4, GPIO_PIN_14, GPIO_PIN_15, hspi1);

    // config cc1101
    cc1101_begin(cc, MOD_ASK_OOK, 433.8, 10);
    cc1101_setModulation(cc, MOD_ASK_OOK);
    cc1101_setFrequency(cc, 433.8);
    cc1101_setDataRate(cc, 10);
    cc1101_setOutputPower(cc, 10);
    cc1101_setPacketLengthMode(cc, PKT_LEN_MODE_VARIABLE, 255);
    cc1101_setAddressFilteringMode(cc, ADDR_FILTER_MODE_NONE);
    cc1101_setPreambleLength(cc, 16);
    cc1101_setSyncWord(cc, 0x6996);
    cc1101_setSyncMode(cc, SYNC_MODE_16_16);
    cc1101_setCrc(cc, false);

    //register EXTI callback
    interrupts[0].interrupt = cc1101_exti_callback_gd0;
    interrupts[0].gpio = GPIO_PIN_14;
    interrupts[0].arg = cc;
    interrupts[1].interrupt = cc1101_exti_callback_gd2;
    interrupts[1].gpio = GPIO_PIN_15;
    interrupts[1].arg = cc;

    cc1101_start_receive(cc);
    cc1101_receiveCallback(cc, on_receive);
    while (1) {
        HAL_Delay(1);
    }
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


}

void on_receive(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
    printf("received %d:", len);
    for (int i = 0; i < len; i++) {
        printf("%c", data[i]);
    }
    printf("rssi: %d, lq: %d", rssi, lq);
    printf("\r\n");
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
}

void transmitter(void) {
    uint8_t transmit[40];
    int n = 0;
    while (1) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
        sprintf(transmit, "bla bla bla ble ble ble blu blu %d", n);
        cc1101_transmit(cc, transmit, 40, 0);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
        HAL_Delay(500);
        n++;
    }
}

void receiver(void) {
    uint8_t receive[255] = {0};
    while (1) {

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
        cc1101_receive(cc, receive, 40, 0);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);
        printf("received string:");
        for (int i = 0; i < 40; i++) {
            printf("%c", receive[i]);
        }
        printf("\r\n");
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
        memset(receive, 0, 255);
        HAL_Delay(10);
    }
}


void HAL_GPIO_EXTI_Callback(uint16_t gpio) {
    for (uint32_t i = 0; i < (sizeof(interrupts) / sizeof(interrupts[0])); i++) {
        if (interrupts[i].gpio == gpio) {
            if (HAL_GPIO_ReadPin(GPIOC, gpio)) {
                interrupts[i].interrupt(gpio, interrupts[i].arg);
            }
        }
    }
}
