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
uint8_t sensor_dataChannels[DATA_CHANNEL_COUNT] = {0};


struct Subscriber subscribers[SUBSCRIBER_COUNT] = {0};

UART_HandleTypeDef *uart;
ADC_HandleTypeDef *adc;
struct cc1101 *cc;
struct ld2410b ld;

#include "impl/sensorImpl.h"

void sensor_init(struct cc1101 *ccPassed, UART_HandleTypeDef *uartPassed, ADC_HandleTypeDef *adcPassed) {
    uart = uartPassed;
    adc = adcPassed;
    cc = ccPassed;
    hw_enable_ld(true);
    ld2410b_create(&ld, uart);
    // fake data request to fill sensor_dataChannels
    get_data(0);
}

void sensor_send_data(uint16_t value, uint8_t dataCh) {

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
    if (!is_sensor()) {
        return;
    }
    for (int i = 0; i < DATA_CHANNEL_COUNT; i++) {
        if (sensor_dataChannels[i] != 0) {
            uint16_t data = get_data(i+1);
            struct SensorConfig config = hw_get_sensor_config();
            sensor_send_data(data, config.mapping[i]);
        }
    }
}

