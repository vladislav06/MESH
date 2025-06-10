//
// Created by vm on 25.3.6.
//

#include "sensor.h"
#include "protocol.h"
#include "hw.h"
#include "routing.h"
#include "cc1101.h"
#include "ld2410b.h"

// 1 by default
uint8_t sensor_place = 1;
uint8_t sensor_sensorCh = 1;
// data will be posted to channels 1 and 2
uint8_t sensor_dataChannels[DATA_CHANNEL_COUNT] = {1, 2};


struct Subscriber subscribers[SUBSCRIBER_COUNT] = {0};

UART_HandleTypeDef *uart;
ADC_HandleTypeDef *adc;
struct cc1101 *cc;
struct ld2410b ld;

void sensor_init(struct cc1101 *ccPassed, UART_HandleTypeDef *uartPassed, ADC_HandleTypeDef *adcPassed) {
    uart = uartPassed;
    adc = adcPassed;
    cc = ccPassed;
    hw_enable_ld(true);
    ld2410b_create(&ld, uart);
}

void sensor_send_data(uint16_t value, uint8_t channel) {
    uint8_t dataCh = sensor_dataChannels[channel];

    for (int sub = 0; sub < SUBSCRIBER_COUNT; sub++) {
        if (subscribers[sub].id != 0) {
            struct PacketCD cd = {
                    .header.sourceId=hw_id(),
                    .header.originalSource=hw_id(),
                    .header.finalDestination=subscribers[sub].id,
                    .header.destinationId=routing_getRouteById(subscribers[sub].id),
                    .header.packetType=CD,
                    .header.magic=MAGIC,
                    .header.size=PCK_SIZE(PacketCD),
                    .dataCh=dataCh,
                    .sensorCh=sensor_sensorCh,
                    .place=sensor_place,
                    .value=value,
            };
            calc_crc((struct Packet *) &cd);
            cc1101_transmit_sync(cc, (uint8_t *) &cd, sizeof(struct PacketCD), 0);
        }
    }
}

void sensor_send() {
    switch (hw_id()) {
        case 0x3133:
//        case 0x4f4d:
            sensor_send_data(hw_read_analog(adc, 0), 0);
            break;
        case 0x3f50:
        case 0x2850:
            ld2410b_processReport(&ld);
            sensor_send_data(ld.target_distanceStationary, 1);
    }
}
