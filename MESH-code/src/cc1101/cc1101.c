//
// Created by vm on 25.6.4.
//
#include <math.h>
#include <stdio.h>
#include "cc1101.h"
#include "stm32l0xx_hal.h"
#include "utils.h"
#include "memory.h"
#include "stdarg.h"
#include "hw.h"

#define log2(x) (log(x) / log(2))
#define min(x, y) (((x) < (y)) ? (x) : (y))
#define PIN_UNUSED                0xff


#define CC1101_FIFO_SIZE          64    /* 64 B */
#define CC1101_CRYSTAL_FREQ       27    /* 26 MHz */
#define CC1101_PKT_MAX_SIZE       61    /* 61 B, 1B for address, 2B for status */

#define CC1101_WRITE              0x00
#define CC1101_READ               0x80
#define CC1101_BURST              0x40

#define CC1101_PARTNUM            0x00
#define CC1101_VERSION            0x14

/* Command strobes */
#define CC1101_CMD_RES            0x30  /* Reset chip */
#define CC1101_CMD_RX             0x34  /* Enable RX */
#define CC1101_CMD_TX             0x35  /* Enable TX */
#define CC1101_CMD_IDLE           0x36  /* Enable IDLE */
#define CC1101_CMD_FRX            0x3a  /* Flush the RX FIFO buffer */
#define CC1101_CMD_FTX            0x3b  /* Flush the TX FIFO buffer */
#define CC1101_CMD_NOP            0x3d  /* No operation */

/* Registers */
#define CC1101_REG_IOCFG2         0x00
#define CC1101_REG_IOCFG1         0x01
#define CC1101_REG_IOCFG0         0x02
#define CC1101_REG_FIFOTHR        0x03
#define CC1101_REG_SYNC1          0x04  /* Sync Word, High uint8_t */
#define CC1101_REG_SYNC0          0x05  /* Sync Word, Low uint8_t */
#define CC1101_REG_PKTLEN         0x06
#define CC1101_REG_PKTCTRL1       0x07
#define CC1101_REG_PKTCTRL0       0x08  /* Packet Automation Control */

#define CC1101_REG_CHANNR         0x0a
#define CC1101_REG_FREQ2          0x0d
#define CC1101_REG_FREQ1          0x0e
#define CC1101_REG_MDMCFG4        0x10
#define CC1101_REG_MDMCFG3        0x11
#define CC1101_REG_MDMCFG2        0x12  /* Modem Configuration */
#define CC1101_REG_MDMCFG1        0x13
#define CC1101_REG_MDMCFG0        0x14
#define CC1101_REG_DEVIATN        0x15
#define CC1101_REG_FOCCFG         0x19
#define CC1101_REG_FREQ0          0x0f

#define CC1101_REG_MCSM2          0x16
#define CC1101_REG_MCSM1          0x17
#define CC1101_REG_MCSM0          0x18
#define CC1101_REG_FREND0         0x22  /* Front End TX Configuration */

#define CC1101_REG_PATABLE        0x3e
#define CC1101_REG_FIFO           0x3f


/* CCStatus registers */
#define CC1101_REG_PARTNUM        0x30
#define CC1101_REG_VERSION        0x31
#define CC1101_REG_TXbytes        0x3a
#define CC1101_REG_RXbytes        0x3b
#define CC1101_REG_RCCTRL0_STATUS 0x3d
#define CC1101_REG_RSSI           0x34


/* internal functions */
void _chipSelect(struct cc1101 *instance);

void _waitReady(struct cc1101 *instance);

void _chipDeselect(struct cc1101 *instance);

uint8_t _readRegField(struct cc1101 *instance, uint8_t addr, uint8_t hi, uint8_t lo);

uint8_t _readReg(struct cc1101 *instance, uint8_t addr);

void _readRegBurst(struct cc1101 *instance, uint8_t addr, uint8_t *buff, size_t size);

void _writeRegField(struct cc1101 *instance, uint8_t addr, uint8_t data, uint8_t hi, uint8_t lo);

void _writeReg(struct cc1101 *instance, uint8_t addr, uint8_t data);

void _writeRegBurst(struct cc1101 *instance, uint8_t addr, uint8_t *data, size_t size);

void _sendCmd(struct cc1101 *instance, uint8_t addr);

void _setRegs(struct cc1101 *instance);

void _hardReset(struct cc1101 *instance);

void _flushRxBuffer(struct cc1101 *instance);

void _flushTxBuffer(struct cc1101 *instance);

enum CCState _getState(struct cc1101 *instance);

void _setState(struct cc1101 *instance, enum CCState);

void _saveStatus(struct cc1101 *instance, uint8_t status);

void _receive_interrupt(struct cc1101 *instance);

void _transmit_interrupt(struct cc1101 *instance);

void _switchToDefaultState(struct cc1101 *instance);

uint8_t _waitFor(struct cc1101 *instance, int count, ...);

struct cc1101 *cc1101_create(uint16_t cs, uint16_t gd0, uint16_t gd2, SPI_HandleTypeDef *hspi) {
    struct cc1101 *instance = malloc(sizeof(struct cc1101));
    instance->cs = cs;
    instance->gd0 = gd0;
    instance->gd2 = gd2;
    instance->currentState = STATE_IDLE;
    instance->config.mod = MOD_2FSK;
    instance->config.pktLenMode = PKT_LEN_MODE_FIXED;
    instance->config.addrFilterMode = ADDR_FILTER_MODE_NONE;
    instance->config.freq = 433.5;
    instance->config.drate = 4.0;
    instance->config.power = 0;
    instance->recvCallbackEn = 0;
    instance->spi = hspi;
    instance->rxPckLen = 0;
    instance->rxPckLenProg = 0;
    instance->trState = TRX_DISABLE;
    instance->rxFiFoThresSize = 0;
    instance->txPckLen = 0;
    instance->txPckLenProg = 0;
    instance->defaultState = DEF_IDLE;
    memset(instance->rxBuf, 0, 255);
    memset(instance->txBuf, 0, 255);


    return instance;
}

enum CCStatus cc1101_begin(struct cc1101 *instance, struct CCconfig config) {
    enum CCStatus status;

    _chipDeselect(instance);

    _hardReset(instance);
    HAL_Delay(10);

    uint8_t partnum = cc1101_getChipPartNumber(instance);
    uint8_t version = cc1101_getChipVersion(instance);
    if (partnum != CC1101_PARTNUM || version != CC1101_VERSION) {
        return STATUS_CHIP_NOT_FOUND;
    }

    _setRegs(instance);

    cc1101_setConfig(instance, config);

    _setState(instance, STATE_IDLE);
    _flushRxBuffer(instance);
    _flushTxBuffer(instance);

    return STATUS_OK;
}


void cc1101_setConfig(struct cc1101 *instance, struct CCconfig config) {
    //wait for idle
    while (_waitFor(instance, 3, TX_STOP, RX_STOP, TRX_DISABLE)) {
//    while (instance->trState != TX_STOP && instance->trState != RX_STOP) {
        HAL_Delay(1);
    }

    //lock
    instance->trState = TRX_DISABLE;
    instance->config = config;
    _setState(instance, STATE_IDLE);

    cc1101_setModulation(instance, instance->config.mod);
    cc1101_setFrequency(instance, instance->config.freq);
    cc1101_setOutputPower(instance, instance->config.power);
    cc1101_setDataRate(instance, instance->config.drate);
    cc1101_setAddressFilteringMode(instance, instance->config.addrFilterMode);
    cc1101_setPreambleLength(instance, instance->config.preambleLen);
    cc1101_setFrequencyDeviation(instance, instance->config.freqDev);
    cc1101_setRxBandwidth(instance, instance->config.bandwidth);
    cc1101_setPacketLengthMode(instance, instance->config.pktLenMode, instance->config.packetMaxLen);
    cc1101_setCrc(instance, instance->config.crc);
    cc1101_setSyncWord(instance, instance->config.syncWord);
    cc1101_setSyncMode(instance, instance->config.syncMode);

}

void cc1101_setModulation(struct cc1101 *instance, enum CCModulation mod) {
    _writeRegField(instance, CC1101_REG_MDMCFG2, (uint8_t) mod, 6, 4);
}

enum CCStatus cc1101_setFrequency(struct cc1101 *instance, double freq) {
    if (!((freq >= 300.0 && freq <= 348.0) ||
          (freq >= 387.0 && freq <= 464.0) ||
          (freq >= 779.0 && freq <= 928.0))) {
        return STATUS_INVALID_PARAM;
    }

    _setState(instance, STATE_IDLE);

    uint32_t f = ((freq * 65536.0) / CC1101_CRYSTAL_FREQ);
    _writeReg(instance, CC1101_REG_FREQ0, f & 0xff);
    _writeReg(instance, CC1101_REG_FREQ1, (f >> 8) & 0xff);
    _writeReg(instance, CC1101_REG_FREQ2, (f >> 16) & 0xff);
    return STATUS_OK;
}

enum CCStatus cc1101_setFrequencyDeviation(struct cc1101 *instance, double dev) {
    double xosc = CC1101_CRYSTAL_FREQ * 1000;

    int devMin = (xosc / (1 << 17)) * (8 + 0) * 1;
    int devMax = (xosc / (1 << 17)) * (8 + 7) * (1 << 7);

    if (dev < devMin || dev > devMax) {
        return STATUS_INVALID_PARAM;
    }

    uint8_t bestE = 0, bestM = 0;
    double diff = devMax;

    for (uint8_t e = 0; e <= 7; e++) {
        for (uint8_t m = 0; m <= 7; m++) {
            double t = (xosc / (double) (1ULL << 17)) * (8 + m) * (double) (1ULL << e);
            if (fabs(dev - t) < diff) {
                diff = fabs(dev - t);
                bestE = e;
                bestM = m;
            }
        }
    }

    _writeRegField(instance, CC1101_REG_DEVIATN, bestM, 2, 0);
    _writeRegField(instance, CC1101_REG_DEVIATN, bestE, 6, 4);

    return STATUS_OK;
}

void cc1101_setChannel(struct cc1101 *instance, uint8_t ch) {
    _writeReg(instance, CC1101_REG_CHANNR, ch);
}

enum CCStatus cc1101_setChannelSpacing(struct cc1101 *instance, double sp) {
    double xosc = CC1101_CRYSTAL_FREQ * 1000;

    int spMin = (xosc / (double) (1ULL << 18)) * (256. + 0.) * 1.;
    int spMax = (xosc / (double) (1ULL << 18)) * (256. + 255.) * 8.;

    if (sp < spMin || sp > spMax) {
        return STATUS_INVALID_PARAM;
    }

    uint8_t bestE = 0, bestM = 0;
    double diff = spMax;

    for (uint8_t e = 0; e <= 3; e++) {
        for (uint16_t m = 0; m <= 255; m++) {
            double t = (xosc / (double) (1ULL << 18)) * (256. + m) * (double) (1ULL << e);
            if (fabs(sp - t) < diff) {
                diff = fabs(sp - t);
                bestE = e;
                bestM = m;
            }
        }
    }

    _writeReg(instance, CC1101_REG_MDMCFG0, bestM);
    _writeRegField(instance, CC1101_REG_MDMCFG1, bestE, 1, 0);

    return STATUS_OK;
}

enum CCStatus cc1101_setDataRate(struct cc1101 *instance, double drate) {

    static const double range[][2] = {
            [MOD_2FSK]    = {0.6, 500.0},  /* 0.6 - 500 kBaud */
            [MOD_GFSK]    = {0.6, 250.0},
            [2]           = {0.0, 0.0},  /* gap */
            [MOD_ASK_OOK] = {0.6, 250.0},
            [MOD_4FSK]    = {0.6, 300.0},
            [5]           = {0.0, 0.0},  /* gap */
            [6]           = {0.0, 0.0},  /* gap */
            [MOD_MSK]     = {26.0, 500.0}
    };

    if (drate < range[instance->config.mod][0] || drate > range[instance->config.mod][1]) {
        return STATUS_INVALID_PARAM;
    }

    uint32_t xosc = CC1101_CRYSTAL_FREQ * 1000;
    uint8_t e = log2((drate * (double) (1ULL << 20)) / xosc);
    uint32_t m = round(drate * ((double) (1ULL << (28 - e)) / xosc) - 256.);

    if (m == 256) {
        m = 0;
        e++;
    }

    _writeRegField(instance, CC1101_REG_MDMCFG4, e, 3, 0);
    _writeReg(instance, CC1101_REG_MDMCFG3, (uint8_t) m);

    return STATUS_OK;
}

enum CCStatus cc1101_setRxBandwidth(struct cc1101 *instance, double bw) {
    /*
      CC1101 supports the following channel filter bandwidths [kHz]:
      (assuming a 26 MHz crystal).

    \ E  0     1     2     3
    M +----------------------
    0 | 812 | 406 | 203 | 102
    1 | 650 | 335 | 162 |  81
    2 | 541 | 270 | 135 |  68
    3 | 464 | 232 | 116 |  58

    */

    int bwMin = (CC1101_CRYSTAL_FREQ * 1000) / (8 * (4 + 3) * (1 << 3));
    int bwMax = (CC1101_CRYSTAL_FREQ * 1000) / (8 * (4 + 0) * (1 << 0));

    if (bw < bwMin || bw > bwMax) {
        return STATUS_INVALID_PARAM;
    }

    uint8_t bestE = 0, bestM = 0;
    double diff = bwMax;

    for (uint8_t e = 0; e <= 3; e++) {
        for (uint8_t m = 0; m <= 3; m++) {
            double t = (double) (CC1101_CRYSTAL_FREQ * 1000) / (8 * (4 + m) * (1 << e));
            if (fabs(bw - t) < diff) {
                diff = fabs(bw - t);
                bestE = e;
                bestM = m;
            }
        }
    }

    _writeRegField(instance, CC1101_REG_MDMCFG4, bestE, 7, 6);
    _writeRegField(instance, CC1101_REG_MDMCFG4, bestM, 5, 4);

    return STATUS_OK;
}

void cc1101_setOutputPower(struct cc1101 *instance, int8_t power) {

    static const uint8_t powers[][8] = {
            [0 /* 315 Mhz */ ] = {0x12, 0x0d, 0x1c, 0x34, 0x51, 0x85, 0xcb, 0xc2},
            [1 /* 433 Mhz */ ] = {0x12, 0x0e, 0x1d, 0x34, 0x60, 0x84, 0xc8, 0xc0},
            [2 /* 868 Mhz */ ] = {0x03, 0x0f, 0x1e, 0x27, 0x50, 0x81, 0xcb, 0xc2},
            [3 /* 915 MHz */ ] = {0x03, 0x0e, 0x1e, 0x27, 0x8e, 0xcd, 0xc7, 0xc0}
    };

    uint8_t powerIdx, freqIdx;

    if (instance->config.freq <= 348.0) {
        freqIdx = 0;
    } else if (instance->config.freq <= 464.0) {
        freqIdx = 1;
    } else if (instance->config.freq <= 855.0) {
        freqIdx = 2;
    } else {
        freqIdx = 3;
    }

    if (power <= -30) {
        powerIdx = 0;
    } else if (power <= -20) {
        powerIdx = 1;
    } else if (power <= -15) {
        powerIdx = 2;
    } else if (power <= -10) {
        powerIdx = 3;
    } else if (power <= 0) {
        powerIdx = 4;
    } else if (power <= 5) {
        powerIdx = 5;
    } else if (power <= 7) {
        powerIdx = 6;
    } else {
        powerIdx = 7;
    }


    if (instance->config.mod == MOD_ASK_OOK) {
        /* No shaping. Use only the first 2 entries in the power table. */
        uint8_t data[2] = {0x00, powers[freqIdx][powerIdx]};
        _writeRegBurst(instance, CC1101_REG_PATABLE, data, sizeof(data));
        _writeRegField(instance, CC1101_REG_FREND0, 1, 2, 0);  /* PA_POWER = 1 */
    } else {
        _writeReg(instance, CC1101_REG_PATABLE, powers[freqIdx][powerIdx]);
        _writeRegField(instance, CC1101_REG_FREND0, 0, 2, 0);  /* PA_POWER = 0 */
    }
}

enum CCStatus cc1101_setPreambleLength(struct cc1101 *instance, uint8_t length) {
    uint8_t data;

    switch (length) {
        case 16:
            data = 0;
            break;
        case 24:
            data = 1;
            break;
        case 36:
            data = 2;
            break;
        case 48:
            data = 3;
            break;
        case 64:
            data = 4;
            break;
        case 96:
            data = 5;
            break;
        case 128:
            data = 6;
            break;
        case 192:
            data = 7;
            break;
        default:
            return STATUS_INVALID_PARAM;
    }

    _writeRegField(instance, CC1101_REG_MDMCFG1, data, 6, 4);
    return STATUS_OK;
}

void cc1101_setSyncWord(struct cc1101 *instance, uint16_t sync) {
    _writeReg(instance, CC1101_REG_SYNC1, sync >> 8);
    _writeReg(instance, CC1101_REG_SYNC0, sync & 0xff);
}

void cc1101_setSyncMode(struct cc1101 *instance, enum CCSyncMode mode) {
    _writeRegField(instance, CC1101_REG_MDMCFG2, (uint8_t) mode, 2, 0);
}

void cc1101_setPacketLengthMode(struct cc1101 *instance, enum CCPacketLengthMode mode, uint8_t length) {
    instance->config.pktLenMode = mode;
    _writeRegField(instance, CC1101_REG_PKTCTRL0, (uint8_t) mode, 1, 0);
    _writeReg(instance, CC1101_REG_PKTLEN, length);
}

void cc1101_setAddressFilteringMode(struct cc1101 *instance, enum CCAddressFilteringMode mode) {
    _writeRegField(instance, CC1101_REG_PKTCTRL1, (uint8_t) mode, 1, 0);
}

void cc1101_setCrc(struct cc1101 *instance, uint8_t enable) {
    _writeRegField(instance, CC1101_REG_PKTCTRL0, (uint8_t) enable, 2, 2);
}

enum CCStatus cc1101_transmit_async(struct cc1101 *instance, uint8_t *data, size_t length, uint8_t addr) {
    cc1101_start_transmit(instance);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);

    uint8_t bytesSent = 0;

    if (length > 254) {
        return STATUS_LENGTH_TOO_BIG;
    }
    if (length < CC1101_FIFO_SIZE) {
        //transmit in sync mode
        cc1101_transmit_sync(instance, data, length, 0);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);
        return STATUS_OK;
    }

    instance->trState = TX_START;

    //copy data to transmit buffer
    memcpy(instance->txBuf, data, length);
    instance->txPckLen = length;
    instance->txPckLenProg = 0;

    uint8_t bytesToWrite = min(CC1101_FIFO_SIZE - 1, length);
    _writeReg(instance, CC1101_REG_FIFO, (uint8_t) length);
    _writeRegBurst(instance, CC1101_REG_FIFO, data, bytesToWrite);
    instance->txPckLenProg += bytesToWrite;

    _setState(instance, STATE_TX);
    return STATUS_OK;
}

enum CCStatus cc1101_transmit_sync(struct cc1101 *instance, uint8_t *data, size_t length, uint8_t addr) {
    //wait for idle
    while (_waitFor(instance, 4, TX_STOP, RX_STOP, RX_PRE_END, TRX_DISABLE)) {
//    while (instance->trState != TX_STOP && instance->trState != RX_STOP && instance->trState != RX_PRE_END) {
        HAL_Delay(1);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);
    //save state
    enum CCTRXState prevState = instance->trState;
    //lock
    instance->trState = TRX_DISABLE;

    instance->txPckLen = length;
    instance->txPckLenProg = 0;

    uint8_t bytesToWrite = min(CC1101_FIFO_SIZE - 1, length);
    _writeReg(instance, CC1101_REG_FIFO, (uint8_t) length);
    _writeRegBurst(instance, CC1101_REG_FIFO, data, bytesToWrite);
    instance->txPckLenProg += bytesToWrite;

    _setState(instance, STATE_IDLE);


    _setState(instance, STATE_TX);

    while (instance->txPckLenProg < length) {
        uint8_t bytesInfifo = _readRegField(instance, CC1101_REG_TXbytes, 6, 0);

        if (bytesInfifo < (CC1101_FIFO_SIZE - 1)) {
            bytesToWrite = min((uint8_t) (length - instance->txPckLenProg),
                               (uint8_t) (CC1101_FIFO_SIZE - bytesInfifo));
            _writeRegBurst(instance, CC1101_REG_FIFO, data + instance->txPckLenProg, bytesToWrite);
            instance->txPckLenProg += bytesToWrite;
        }
    }

    while (_getState(instance) != STATE_IDLE) {
        delay_micros(50);
    }

//    _flushRxBuffer(instance);
//    _flushTxBuffer(instance);
    instance->trState = prevState;
//    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);
    _switchToDefaultState(instance);
    return STATUS_OK;
}

uint16_t
cc1101_measure_round_trip_master(struct cc1101 *instance, uint8_t *data, size_t length, TIM_HandleTypeDef *timer) {
    //wait for idle
    while (_waitFor(instance, 4, TX_STOP, RX_STOP, RX_PRE_END, TRX_DISABLE)) {
        HAL_Delay(1);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);

    //save state
    enum CCTRXState prevState = instance->trState;
    //lock
    instance->trState = TRX_DISABLE;

    //save state
    uint8_t MCSM1 = _readReg(instance, CC1101_REG_MCSM1);
    uint8_t MCSM0 = _readReg(instance, CC1101_REG_MCSM0);
    uint8_t IOCFG0 = _readReg(instance, CC1101_REG_IOCFG0);
    uint8_t FIFOTHR = _readReg(instance, CC1101_REG_FIFOTHR);

    //set required fields
    //from TX straight to RX

    _writeRegField(instance, CC1101_REG_MCSM1, 0, 3, 2);
    _writeRegField(instance, CC1101_REG_MCSM1, 3, 1, 0);

    _writeRegField(instance, CC1101_REG_MCSM0, 1, 5, 4);
    enableInterrupts = 0;

    //assert when end of the packet is reached
    _writeReg(instance, CC1101_REG_IOCFG0, 1);

    _writeRegField(instance, CC1101_REG_FIFOTHR, 7, 3, 0);

    _setState(instance, STATE_IDLE);

    _writeReg(instance, CC1101_REG_FIFO, (uint8_t) length);
    _writeRegBurst(instance, CC1101_REG_FIFO, data, length);

    //_setState(instance, STATE_TX);
    _sendCmd(instance, CC1101_CMD_TX);
    hw_set_D4(1);
    timer->Instance->CNT = 0;
    delay_micros(10000);
    //wait for GD0
//    while (!(GPIOC->IDR & GPIO_PIN_14)) {
//    }
    //round trip in timer ticks
    uint16_t timerValue = timer->Instance->CNT;
    hw_set_D4(0);

    //revert all back
    _setState(instance, STATE_IDLE);
    while (_getState(instance) != STATE_IDLE) {
        delay_micros(50);
    }
    _flushRxBuffer(instance);
    _flushTxBuffer(instance);

    _writeReg(instance, MCSM1, CC1101_REG_MCSM1);
    _writeReg(instance, MCSM0, CC1101_REG_MCSM0);
    _writeReg(instance, IOCFG0, CC1101_REG_IOCFG0);
    _writeReg(instance, FIFOTHR, CC1101_REG_FIFOTHR);


    instance->trState = prevState;
    _switchToDefaultState(instance);


    return timerValue;
}

int16_t cc1101_receive_sync(struct cc1101 *instance, uint8_t *data, size_t length, uint8_t *rssi, uint8_t *lq,
                            uint32_t timeout) {
    //wait for idle
    while (_waitFor(instance, 4, TX_STOP, RX_STOP, RX_PRE_END, TRX_DISABLE)) {
//        while (instance->trState != TX_STOP && instance->trState != RX_STOP && instance->trState != RX_PRE_END) {
        HAL_Delay(1);
    }
    //save state
    enum CCTRXState prevState = instance->trState;
    //lock
    instance->trState = TRX_DISABLE;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 1);
    _writeReg(instance, CC1101_REG_IOCFG2, 6);

    if (length > 255) {
        return -1;
    }
    _setState(instance, STATE_IDLE);
    delay_micros(500);
    _flushRxBuffer(instance);
    delay_micros(500);
    _setState(instance, STATE_RX);
    delay_micros(500);

    uint8_t pktLength = 0;
    uint32_t time = 0;
    while (!HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15));
    while (_readRegField(instance, CC1101_REG_RXbytes, 6, 0) != 0);
    while (pktLength == 0) {
        //idk why, but first read contains garbage
        pktLength = _readReg(instance, CC1101_REG_FIFO);
        pktLength = _readReg(instance, CC1101_REG_FIFO);

        delay_micros(100);
        time++;
        if (time >= timeout) {
            instance->trState = prevState;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 0);
            _setState(instance, STATE_IDLE);
            _flushRxBuffer(instance);
            _switchToDefaultState(instance);
            return -1;
        }
    }

    if (pktLength > length) {
        return STATUS_LENGTH_TOO_SMALL;
    }

    uint8_t bytesInFifo = _readRegField(instance, CC1101_REG_RXbytes, 6, 0);
    uint8_t bytesRead = 0;

    /*
      For packet lengths less than 64 uint8_ts it is recommended to wait until
      the complete packet has been received before reading it out of the RX FIFO.
    */
    if (pktLength <= CC1101_PKT_MAX_SIZE) {
        while (bytesInFifo < pktLength) {
            delay_micros(15 * 8);
            bytesInFifo = _readRegField(instance, CC1101_REG_RXbytes, 6, 0);
        }
    }

    while (bytesRead < pktLength) {
        while (bytesInFifo == 0) {
            delay_micros(15);
            bytesInFifo = _readRegField(instance, CC1101_REG_RXbytes, 6, 0);
        }

        uint8_t bytesToRead = min((uint8_t) (pktLength - bytesRead), bytesInFifo);
        _readRegBurst(instance, CC1101_REG_FIFO, data + bytesRead, bytesToRead);
        bytesRead += bytesToRead;
    }
    //packet contain rssi and lq values
    *rssi = _readReg(instance, CC1101_REG_FIFO);
    *lq = _readReg(instance, CC1101_REG_FIFO) & 0x7F;
    _setState(instance, STATE_IDLE);
    while (_getState(instance) != STATE_IDLE) {
        delay_micros(50);
    }
    _flushRxBuffer(instance);

    //switch to default state
    instance->trState = prevState;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 0);
    _switchToDefaultState(instance);
    return pktLength;
}

void
cc1101_receiveCallback(struct cc1101 *instance, void (*func)(uint8_t *data, uint8_t len, uint8_t rssi, uint8_t lq)) {
    _flushRxBuffer(instance);
    instance->recvCallbackEn = 1;
    instance->receive_callback = func;
}

// Internal function implementation
static uint8_t a = 0;

void cc1101_exti_callback_gd0(uint16_t gpio, void *arg) {
    // cc is not initialised

    if (arg == 0) {
        return;
    }

    struct cc1101 *instance = (struct cc1101 *) arg;
    if (instance->trState & RX) {
        if (instance->gd0 == gpio && instance->recvCallbackEn == 1) {
            _receive_interrupt(instance);
        }
    } else if (instance->currentState == STATE_TX && (instance->trState == TX_STOP || instance->trState == TX_START)) {
        if (instance->gd0 == gpio) {
            _transmit_interrupt(instance);
        }
    }
}

void cc1101_exti_callback_gd2(uint16_t gpio, void *arg) {
    // cc is not initialised
    if (arg == 0) {
        return;
    }

    struct cc1101 *instance = (struct cc1101 *) arg;
    if (instance->trState == RX_START || instance->trState == RX_STOP) {
        if (instance->gd2 == gpio && instance->recvCallbackEn == 1) {
            _receive_interrupt(instance);
        }
    }
    if (instance->trState == TX_END) {
        if (instance->gd2 == gpio) {
            _transmit_interrupt(instance);
        }
    }
}

void _receive_interrupt(struct cc1101 *instance) {
    // if new packet, get packet length and start reception
    if (instance->trState == RX_STOP) {
        instance->trState = RX_START;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 1);

        //read rx packet len
        uint8_t pktLength = 0;
        while (pktLength == 0) {
            pktLength = _readReg(instance, CC1101_REG_FIFO);
        }
        //packet contain rssi and lq values
        instance->rxPckLen = pktLength + 2;
        instance->rxPckLenProg = 0;
    }
    uint8_t bytesInFifo = _readRegField(instance, CC1101_REG_RXbytes, 6, 0);

    enum CCState state = instance->currentState;
    if (state == STATE_RXFIFO_OVERFLOW) {
        _flushRxBuffer(instance);

        //recover from error
        instance->trState = RX_STOP;
        _setState(instance, STATE_IDLE);
        _flushRxBuffer(instance);
        cc1101_start_receive(instance);
        return;
    }

    uint8_t bytesToRead = 0;
    // decide if packet is being fully received or only partially
    // if already received bytes that are stored in fifo are enough to fully receive packet, do it
    if (instance->rxPckLenProg + bytesInFifo >= instance->rxPckLen) {
        //read entire buffer and end reception
        uint8_t leftToRead = instance->rxPckLen - instance->rxPckLenProg;
        bytesToRead = leftToRead;
        instance->trState = RX_PRE_END;
    } else {
        //read that many bytes that the rest of the packet can trigger threshold
        uint8_t leftToRead = instance->rxPckLen - instance->rxPckLenProg;
        bytesToRead = min(leftToRead, bytesInFifo);
//        instance->trState = RECEIVE_BEGIN;
    }
    // read from buf
    _readRegBurst(instance, CC1101_REG_FIFO, instance->rxBuf + instance->rxPckLenProg, bytesToRead);
    instance->rxPckLenProg += bytesToRead;
    enum CCState stat = _getState(instance);

    // if reception ended, reset state and call callback
    if (instance->trState == RX_PRE_END) {
        enum CCState state = _getState(instance);
        while (state != STATE_IDLE) {
            if (state == STATE_RXFIFO_OVERFLOW) {
                _flushRxBuffer(instance);
            }
            _setState(instance, STATE_IDLE);
            _flushRxBuffer(instance);
            state = _getState(instance);
            delay_micros(50);
        }
        uint8_t rssi = instance->rxBuf[instance->rxPckLen - 2];
        //mask MSB because it is indicating crc
        uint8_t lq = instance->rxBuf[instance->rxPckLen - 1] & 0x7F;

        instance->receive_callback(instance->rxBuf, instance->rxPckLen - 2, rssi, lq);
        instance->trState = RX_END;
        //back to reception
        memset(instance->rxBuf, 0, 255);
        _flushRxBuffer(instance);
        while (_getState(instance) != STATE_IDLE) {
            delay_micros(50);
        }
        _setState(instance, STATE_RX);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, 0);
        instance->trState = RX_STOP;
    }
}

void _transmit_interrupt(struct cc1101 *instance) {
//    printf("%d\r\n",     _readRegField(instance, CC1101_REG_PKTLEN, 6, 0));
    if (instance->trState == TX_START) {
        //refill buffer
        //end of transmission or not
        if (instance->txPckLenProg + (64 - instance->txFiFoThresSize) >= instance->txPckLen) {
            instance->trState = TX_END;
        }
        uint8_t bytesInfifo = _readRegField(instance, CC1101_REG_TXbytes, 6, 0);
        uint8_t bytesToWrite = min(64 - bytesInfifo, instance->txPckLen - instance->txPckLenProg);
        _writeRegBurst(instance, CC1101_REG_FIFO, instance->txBuf + instance->txPckLenProg, bytesToWrite);
        instance->txPckLenProg += bytesToWrite;

        // process TX_END only when gd2 de asserts (when packet is fully sent)
        return;
    }
    if (instance->trState == TX_END) {
        HAL_Delay(2);

        //packet was fully sent
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);
        instance->trState = TX_STOP;

        _setState(instance, STATE_IDLE);
        cc1101_start_receive(instance);
    }
}


enum CCState _getState(struct cc1101 *instance) {
    _sendCmd(instance, CC1101_CMD_NOP);
    return instance->currentState;
}

void _setState(struct cc1101 *instance, enum CCState state) {
    switch (state) {
        case STATE_IDLE:
            _sendCmd(instance, CC1101_CMD_IDLE);
            break;
        case STATE_TX:
            _sendCmd(instance, CC1101_CMD_TX);
            break;
        case STATE_RX:
            _sendCmd(instance, CC1101_CMD_RX);
            break;
        default:
            /* Not supported. */
            return;
    }

    while (_getState(instance) != state) {
        delay_micros(100);
    }
}

void _saveStatus(struct cc1101 *instance, uint8_t status) {
    instance->currentState = (enum CCState) ((status >> 4) & 0b111);
}

void _hardReset(struct cc1101 *instance) {
    _chipDeselect(instance);
    delay_micros(5);
    _chipSelect(instance);
    delay_micros(5);
    _chipDeselect(instance);
    delay_micros(40);

    _chipSelect(instance);
    _waitReady(instance);

    uint8_t data = CC1101_CMD_RES;
    HAL_SPI_Transmit(instance->spi, &data, 1, 1);

    _waitReady(instance);
    _chipDeselect(instance);
}

void _flushRxBuffer(struct cc1101 *instance) {
    if (instance->currentState != STATE_IDLE && instance->currentState != STATE_RXFIFO_OVERFLOW) {
        return;
    }
    _sendCmd(instance, CC1101_CMD_FRX);
}

void _flushTxBuffer(struct cc1101 *instance) {
    if (instance->currentState != STATE_IDLE && instance->currentState != STATE_TXFIFO_UNDERFLOW) {
        return;
    }
    _sendCmd(instance, CC1101_CMD_FTX);
}

uint8_t cc1101_getChipPartNumber(struct cc1101 *instance) {
    return _readReg(instance, CC1101_REG_PARTNUM);
}

uint8_t cc1101_getChipVersion(struct cc1101 *instance) {
    return _readReg(instance, CC1101_REG_VERSION);
}

void cc1101_start_receive(struct cc1101 *instance) {

    //currently inside receive interrupt
    if (instance->trState == RX_PRE_END) {
        return;
    }

    //wait for idle
    while (instance->trState != TX_STOP && instance->trState != RX_STOP) {
        HAL_Delay(1);
    }
    //lock
    instance->trState = TRX_DISABLE;
    _setState(instance, STATE_IDLE);
    while (_getState(instance) != STATE_IDLE) {
        delay_micros(50);
    }

    _flushRxBuffer(instance);
    _flushTxBuffer(instance);

    // set gd0 to assert when RX fifo is above threshold
    _writeReg(instance, CC1101_REG_IOCFG0, 0);
    _writeReg(instance, CC1101_REG_IOCFG2, 6);


    delay_micros(500);

    instance->trState = RX_STOP;
    while (_getState(instance) != STATE_IDLE) {
        delay_micros(500);
    }
    _flushRxBuffer(instance);
    _flushTxBuffer(instance);

    delay_micros(500);
    _setState(instance, STATE_RX);
}

void cc1101_start_transmit(struct cc1101 *instance) {
    //wait for idle
    while (instance->trState != TX_STOP && instance->trState != RX_STOP) {
        HAL_Delay(1);
    }
    //lock
    instance->trState = TRX_DISABLE;

    // set gd0 to assert when TX fifo is above threshold and invert
    _writeReg(instance, CC1101_REG_IOCFG0, (1 << 6) | 2);

    // set gd2 to de asserts at the end of the packet
    _writeReg(instance, CC1101_REG_IOCFG2, 6);

    instance->trState = TX_STOP;
    _setState(instance, STATE_IDLE);
    while (_getState(instance) != STATE_IDLE) {
        delay_micros(50);
    }
    _flushRxBuffer(instance);
    _flushTxBuffer(instance);
}

uint8_t cc1101_getRssi(struct cc1101 *instance) {
    return _readReg(instance, CC1101_REG_RSSI);
}

float cc1101_rssiToDbm(uint8_t rssi) {
    int rssi_dec = rssi;
    if (rssi_dec >= 128) {
        return ((float) rssi_dec - 256.f) / 2 - 74;
    } else {
        return (float) rssi_dec / 2 - 74;
    }
}

uint16_t cc1101_measure_round_trip_responder(struct cc1101 *instance) {
    //wait for idle
    while (_waitFor(instance, 4, TX_STOP, RX_STOP, RX_PRE_END, TRX_DISABLE)) {
        HAL_Delay(1);
    }

    //save state
    enum CCTRXState prevState = instance->trState;
    //lock
    instance->trState = TRX_DISABLE;

    //save state
    uint8_t MCSM1 = _readReg(instance, CC1101_REG_MCSM1);
    uint8_t MCSM0 = _readReg(instance, CC1101_REG_MCSM0);
    uint8_t IOCFG0 = _readReg(instance, CC1101_REG_IOCFG0);
    uint8_t FIFOTHR = _readReg(instance, CC1101_REG_FIFOTHR);

    //set required fields
    //from RX straight to TX
    _writeRegField(instance, CC1101_REG_MCSM1, 2, 3, 2);
    //from TX to IDLE
    _writeRegField(instance, CC1101_REG_MCSM1, 0, 1, 0);

    _writeRegField(instance, CC1101_REG_MCSM0, 1, 5, 4);
    enableInterrupts = 0;

    //assert when end of the packet is reached
    _writeReg(instance, CC1101_REG_IOCFG0, 6);

    _writeRegField(instance, CC1101_REG_FIFOTHR, 7, 3, 0);

    _setState(instance, STATE_IDLE);

    uint8_t data[10];

    _writeReg(instance, CC1101_REG_FIFO, 1);
    _writeRegBurst(instance, CC1101_REG_FIFO, data, 1);

    hw_set_D4(1);
    _sendCmd(instance, CC1101_CMD_RX);

    //wait for GD0/ packet reception
    while (!(GPIOC->IDR & GPIO_PIN_14)) {

    }
    _sendCmd(instance, CC1101_CMD_TX);

    //wait for idle after rx
    enum CCState state=_getState(instance);
    while (state!= STATE_IDLE&&state!= STATE_RXFIFO_OVERFLOW) {
        delay_micros(50);
        state=_getState(instance);
    }
    hw_set_D4(0);

    _flushRxBuffer(instance);
    _flushTxBuffer(instance);

    _writeReg(instance, MCSM1, CC1101_REG_MCSM1);
    _writeReg(instance, MCSM0, CC1101_REG_MCSM0);
    _writeReg(instance, IOCFG0, CC1101_REG_IOCFG0);
    _writeReg(instance, FIFOTHR, CC1101_REG_FIFOTHR);


    instance->trState = prevState;
    _switchToDefaultState(instance);
    return 0;
}


uint8_t _readRegField(struct cc1101 *instance, uint8_t addr, uint8_t hi, uint8_t lo) {
    return (_readReg(instance, addr) >> lo) & ((1 << (hi - lo + 1)) - 1);
}

uint8_t _readReg(struct cc1101 *instance, uint8_t addr) {
    uint8_t header = CC1101_READ | (addr & 0b111111);

    if (addr >= CC1101_REG_PARTNUM && addr <= CC1101_REG_RCCTRL0_STATUS) {
        /* CCStatus registers - access with the burst bit on. */
        header |= CC1101_BURST;
    }

//    SPI.beginTransaction(spiSettings);
    _chipSelect(instance);
    _waitReady(instance);

    uint8_t status = 0;
    HAL_SPI_TransmitReceive(instance->spi, &header, &status, 1, 1);
    _saveStatus(instance, status);

    uint8_t data = 0;
    HAL_SPI_TransmitReceive(instance->spi, &data, &data, 1, 1);

    _chipDeselect(instance);
//    SPI.endTransaction();
    return data;
}

void _readRegBurst(struct cc1101 *instance, uint8_t addr, uint8_t *buff, size_t size) {
    uint8_t header = CC1101_READ | CC1101_BURST | (addr & 0b111111);

    if (addr >= CC1101_REG_PARTNUM && addr <= CC1101_REG_RCCTRL0_STATUS) {
        /* CCStatus registers cannot be accessed with the burst option. */
        return;
    }

//    SPI.beginTransaction(spiSettings);
    _chipSelect(instance);
    _waitReady(instance);

    uint8_t status = 0;
    HAL_SPI_TransmitReceive(instance->spi, &header, &status, 1, 1);
    _saveStatus(instance, status);

    uint8_t data = 0;
    for (size_t i = 0; i < size; i++) {
//        buff[i] = SPI.transfer(0x00);
        HAL_SPI_TransmitReceive(instance->spi, &data, &buff[i], 1, 1);
    }

    _chipDeselect(instance);
//    SPI.endTransaction();
}

void _setRegs(struct cc1101 *instance) {
    // Automatically calibrate when going from IDLE to RX or TX.
    _writeRegField(instance, CC1101_REG_MCSM0, 1, 5, 4);
    _writeRegField(instance, CC1101_REG_MCSM1, 0, 1, 0);
    _writeRegField(instance, CC1101_REG_MCSM1, 0, 3, 2);

//    _writeRegField(instance,CC1101_REG_FOCCFG,)

    // Disable data whitening.
    _writeRegField(instance, CC1101_REG_PKTCTRL0, 0, 6, 6);

    // enable status (rssi lq and crc ok ) appending to the packet
    _writeRegField(instance, CC1101_REG_PKTCTRL1, 1, 2, 2);

    // configure GDx pins
    // set fifo threshold
    _writeRegField(instance, CC1101_REG_FIFOTHR, 7, 3, 0);


    // set gd0 to assert when chip is ready
    _writeReg(instance, CC1101_REG_IOCFG0, 41);
    // set gd2 to assert when chip is ready
    _writeReg(instance, CC1101_REG_IOCFG2, 41);

    instance->rxFiFoThresSize = 32;
    instance->txFiFoThresSize = 33;
}

void _writeRegField(struct cc1101 *instance, uint8_t addr, uint8_t data, uint8_t hi,
                    uint8_t lo) {
    data <<= lo;
    uint8_t current = _readReg(instance, addr);
    uint8_t mask = ((1 << (hi - lo + 1)) - 1) << lo;
    data = (current & ~mask) | (data & mask);
    _writeReg(instance, addr, data);
}

void _writeReg(struct cc1101 *instance, uint8_t addr, uint8_t data) {
    if (addr >= CC1101_REG_PARTNUM && addr <= CC1101_REG_RCCTRL0_STATUS) {
        /* CCStatus registers are read-only. */
        return;
    }

    uint8_t header = CC1101_WRITE | (addr & 0b111111);

//    SPI.beginTransaction(spiSettings);
    _chipSelect(instance);
    _waitReady(instance);


    uint8_t status = 0;
    HAL_SPI_TransmitReceive(instance->spi, &header, &status, 1, 1);
    _saveStatus(instance, status);

    HAL_SPI_TransmitReceive(instance->spi, &data, &status, 1, 1);
    _saveStatus(instance, status);

    _chipDeselect(instance);
//    SPI.endTransaction();
}

void _writeRegBurst(struct cc1101 *instance, uint8_t addr, uint8_t *data, size_t size) {
    if (addr >= CC1101_REG_PARTNUM && addr <= CC1101_REG_RCCTRL0_STATUS) {
        /* CCStatus registers are read-only. */
        return;
    }

    uint8_t header = CC1101_WRITE | CC1101_BURST | (addr & 0b111111);

//    SPI.beginTransaction(spiSettings);
    _chipSelect(instance);
    _waitReady(instance);


    uint8_t status = 0;
    HAL_SPI_TransmitReceive(instance->spi, &header, &status, 1, 1);
    _saveStatus(instance, status);

    for (size_t i = 0; i < size; i++) {
        status = 0;
        HAL_SPI_TransmitReceive(instance->spi, &data[i], &status, 1, 1);
        _saveStatus(instance, status);

//        saveStatus(SPI.transfer(data[i]));
    }

    _chipDeselect(instance);
//    SPI.endTransaction();
}

void _sendCmd(struct cc1101 *instance, uint8_t addr) {
    uint8_t header = CC1101_WRITE | (addr & 0b111111);

//    SPI.beginTransaction(spiSettings);
    _chipSelect(instance);
    _waitReady(instance);

    uint8_t status = 0;
    HAL_SPI_TransmitReceive(instance->spi, &header, &status, 1, 1);
    _saveStatus(instance, status);

    _chipDeselect(instance);
//    SPI.endTransaction();
}

void _chipSelect(struct cc1101 *instance) {
//    digitalWrite(cs, LOW);
    HAL_GPIO_WritePin(GPIOA, instance->cs, 0);

}

void _chipDeselect(struct cc1101 *instance) {
//    digitalWrite(cs, HIGH);
    HAL_GPIO_WritePin(GPIOA, instance->cs, 1);
}

void _waitReady(struct cc1101 *instance) {
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6));
}

void _switchToDefaultState(struct cc1101 *instance) {
    switch (instance->defaultState) {
        case DEF_IDLE:
            _setState(instance, STATE_IDLE);
            break;
        case DEF_RX:
            cc1101_start_receive(instance);
            break;
    }
}

uint8_t _waitFor(struct cc1101 *instance, int count, ...) {
    va_list ap;
    va_start (ap, count);
    uint8_t exp = 1;
    for (int i = 0; i < count; ++i) {
        enum CCTRXState trxState = va_arg(ap, uint32_t);
        exp = exp & (instance->trState != trxState);
    }
    va_end(ap);
    return exp;
}
