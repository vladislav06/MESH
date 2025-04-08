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

    utils_init(htim6);
    HAL_Delay(1000);

    cc = cc1101_create(GPIO_PIN_4, GPIO_PIN_14, 0, hspi1);
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


    uint32_t id = HAL_GetUIDw2();
    if (id == 4128845) {
        transmitter();
    } else {
        receiver();
    }
}

void on_receive(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
    uint8_t receive[255] = {0};

    cc1101_receive(cc, receive, 36, 0);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);

    printf("received string: ");
    for (int i = 0; i < 36; i++) {
        printf("%c", receive[i]);
    }
    printf("\r\n");
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
    memset(receive, 0, 255);


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
    for (uint32_t i = 0; i < (sizeof(exti_callbacks) / sizeof(exti_callbacks[0])); i++) {
        exti_callbacks[i](gpio);
    }
}
