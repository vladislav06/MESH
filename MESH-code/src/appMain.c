//
// Created by vm on 25.6.4.
//
#include "appMain.h"
#include "utils.h"
#include "cc1101.h"
#include "usbd_cdc_if.h"

void appMain(ADC_HandleTypeDef *hadc,
             SPI_HandleTypeDef *hspi1,
             TIM_HandleTypeDef *htim2,
             TIM_HandleTypeDef *htim6,
             UART_HandleTypeDef *huart2) {


//    htim2->Instance->CCR1 = 0x30;
//
//    HAL_TIM_PWM_Start(htim2, TIM_CHANNEL_1);
//
//    while (1) {
//        uint32_t a = 0;
//        htim2->Instance->CCR1 = a;
//        a++;
//    }

    utils_init(htim6);
    HAL_Delay(1000);
    struct cc1101 *cc = cc1101_create(GPIO_PIN_4, GPIO_PIN_14, 0, hspi1);
    cc1101_begin(cc, MOD_GFSK, 433.5, 100);
    uint8_t bla[] = "bla bla lba ble ble ble blu blu";
    uint8_t transmit[36];
    uint8_t receive[255] = {0};

    cc1101_setModulation(cc, MOD_ASK_OOK);
    cc1101_setFrequency(cc, 433.8);
    cc1101_setDataRate(cc, 10);
    cc1101_setOutputPower(cc, 10);

    cc1101_setPacketLengthMode(cc, PKT_LEN_MODE_FIXED, 36);
    cc1101_setAddressFilteringMode(cc, ADDR_FILTER_MODE_NONE);
    cc1101_setPreambleLength(cc, 16);
    cc1101_setSyncWord(cc, 0x6996);
    cc1101_setSyncMode(cc, SYNC_MODE_16_16);
    cc1101_setCrc(cc, false);

//    cc1101_setSyncWord(cc, 0x6996);
//    cc1101_setSyncMode(cc, SYNC_MODE_15_16);
//    cc1101_setCrc(cc, 0);
//    cc1101_setRxBandwidth(cc,100);
//    cc1101_setAddressFilteringMode(cc, ADDR_FILTER_MODE_NONE);
//    cc1101_setOutputPower(cc, 8);
//    cc1101_setPreambleLength(cc, 24);

//    cc1101_setPacketLengthMode(cc, PKT_LEN_MODE_VARIABLE, 36);
//    cc1101_transmit(cc, transmit, sizeof(transmit), 0);
    int n = 0;
    while (1) {
        uint8_t status = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);
        if (status == 1) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);

            memcpy(transmit,bla, 32);

            itoa(n, &transmit[32], 10);
            cc1101_transmit(cc, transmit, 36, 0);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);

            HAL_Delay(500);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);

            cc1101_receive(cc, receive, 36, 0);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);

            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);

            printf("received string:");
            for (int i = 0; i < 36; i++) {
                printf("%c | ", receive[i]);
            }
            printf("\r\n");
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);
            memset(receive, 0, 255);
        }
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
        n++;
    }


}


void HAL_GPIO_EXTI_Callback(uint16_t gpio) {
    for (uint32_t i = 0; i < (sizeof(exti_callbacks) / sizeof(exti_callbacks[0])); i++) {
        exti_callbacks[i](gpio);
    }
}
