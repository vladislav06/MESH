//
// Created by vm on 25.6.4.
//

#ifndef MESH_CODE_CC1101_H
#define MESH_CODE_CC1101_H

#include <stddef.h>
#include <malloc.h>
#include "stdint.h"
#include "stm32l0xx_hal.h"


enum CCStatus {
    STATUS_OK = 0,

    STATUS_LENGTH_TOO_SMALL,
    STATUS_LENGTH_TOO_BIG,
    STATUS_INVALID_PARAM,
    STATUS_CHIP_NOT_FOUND
};

enum CCState {
    STATE_IDLE = 0,  /* IDLE state */
    STATE_RX = 1,  /* Receive mode */
    STATE_TX = 2,  /* Transmit mode */
    STATE_FSTXON = 3,  /* Fast TX ready */
    STATE_CALIBRATE = 4,  /* Freq synthesizer calibration is running */
    STATE_SETTLING = 5,  /* PLL is settling */
    STATE_RXFIFO_OVERFLOW = 6,  /* RX FIFO has overflowed */
    STATE_TXFIFO_UNDERFLOW = 7,  /* TX FIFO has underflowed */
};

enum CCModulation {
    MOD_2FSK = 0,
    MOD_GFSK = 1,
    MOD_ASK_OOK = 3,
    MOD_4FSK = 4,
    MOD_MSK = 7
};

enum CCSyncMode {
    SYNC_MODE_NO_PREAMBLE = 0,  /* No preamble/sync */
    SYNC_MODE_15_16 = 1,  /* 15/16 sync word bits detected */
    SYNC_MODE_16_16 = 2,  /* 16/16 sync word bits detected */
    SYNC_MODE_30_32 = 3,  /* 30/32 sync word bits detected */
    SYNC_MODE_NO_PREAMBLE_CS = 4,  /* No preamble/sync, CS above threshold */
    SYNC_MODE_15_16_CS = 5,  /* 15/16 + carrier-sense above threshold */
    SYNC_MODE_16_16_CS = 6,  /* 16/16 + carrier-sense above threshold */
    SYNC_MODE_30_32_CS = 7,  /* 30/32 + carrier-sense above threshold */
};

enum CCPacketLengthMode {
    PKT_LEN_MODE_FIXED = 0,  /* Length configured in PKTLEN register */
    PKT_LEN_MODE_VARIABLE = 1,  /* Packet length put in the first byte */
    // TODO: PKT_LEN_MODE_INFINITE = 2,  /* Infinite packet length mode */
};

enum CCAddressFilteringMode {
    ADDR_FILTER_MODE_NONE = 0,          /* No address check */
    ADDR_FILTER_MODE_CHECK = 1,         /* Address check, no broadcast */
    ADDR_FILTER_MODE_CHECK_BC_0 = 2,    /* Address check, 0 broadcast */
    ADDR_FILTER_MODE_CHECK_BC_0_255 = 3 /* Address check, 0 and 255 broadcast */
};
enum CCReceiveStatus {
    RECEIVE_BEGIN = 0,
    RECEIVE_END = 1,
};

struct cc1101 {
    SPI_HandleTypeDef *spi;
    uint16_t cs, gd0, gd2;
    enum CCState currentState;
    enum CCModulation mod;
    enum CCPacketLengthMode pktLenMode;
    enum CCAddressFilteringMode addrFilterMode;
    uint8_t recvCallbackEn;
    double freq;
    double drate;
    int8_t power;

    void (*receive_callback)(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq);

    // packer reception
    uint8_t lastPacketLen;
    enum CCReceiveStatus receiveState;
    uint8_t lastPacketRecLen;
    uint8_t rxFiFoThresSize;
    uint8_t rxBuf[256];
    //packet transmission
    uint8_t txFiFoThresSize;

};


void cc1101_exti_callback_gd0(uint16_t gpio, void *arg);

void cc1101_exti_callback_gd2(uint16_t gpio, void *arg);


struct cc1101 *cc1101_create(uint16_t cs, uint16_t gd0, uint16_t gd2, SPI_HandleTypeDef *hspi);


enum CCStatus cc1101_begin(struct cc1101 *instance, enum CCModulation mod, float freq, float drate);

uint8_t cc1101_getChipPartNumber(struct cc1101 *instance);

uint8_t cc1101_getChipVersion(struct cc1101 *instance);

void cc1101_setModulation(struct cc1101 *instance, enum CCModulation mod);

enum CCStatus cc1101_setFrequency(struct cc1101 *instance, double freq);

enum CCStatus cc1101_setFrequencyDeviation(struct cc1101 *instance, double dev);

void cc1101_setChannel(struct cc1101 *instance, uint8_t ch);

enum CCStatus cc1101_setChannelSpacing(struct cc1101 *instance, double sp);

enum CCStatus cc1101_setDataRate(struct cc1101 *instance, double drate);

enum CCStatus cc1101_setRxBandwidth(struct cc1101 *instance, double bw);

void cc1101_setOutputPower(struct cc1101 *instance, int8_t power);

/* Enable CRC calculation in TX and CRC check in RX. */
void cc1101_setCrc(struct cc1101 *instance, uint8_t enable);

void cc1101_setAddressFilteringMode(struct cc1101 *instance, enum CCAddressFilteringMode mode);

void cc1101_setPacketLengthMode(struct cc1101 *instance, enum CCPacketLengthMode mode, uint8_t length);

void cc1101_setSyncMode(struct cc1101 *instance, enum CCSyncMode mode);

enum CCStatus cc1101_setPreambleLength(struct cc1101 *instance, uint8_t length);

void cc1101_setSyncWord(struct cc1101 *instance, uint16_t sync);

enum CCStatus cc1101_transmit(struct cc1101 *instance, uint8_t *data, size_t length, uint8_t addr);

enum CCStatus cc1101_receive(struct cc1101 *instance, uint8_t *data, size_t length, uint8_t addr);

void
cc1101_receiveCallback(struct cc1101 *instance, void (*func)(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq));

void cc1101_start_receive(struct cc1101 *instance);

#endif //MESH_CODE_CC1101_H
