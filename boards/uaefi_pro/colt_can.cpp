#include "pch.h"
#include "colt_can.h"

static uint32_t lastTxTime = 0;

void initColtCan() {
}

void processColtCanTx() {
    if (!engineConfiguration->isCanEnabled) {
        return;
    }

    uint32_t now = getTimeNowUs();
    if (now - lastTxTime < 10000) {
        return;
    }

    lastTxTime = now;

    uint8_t data[8] = {0};

    float rpm = Sensor::get(SensorType::Rpm);
    uint16_t rawRpm = (uint16_t)(rpm * 1024.0f / 1000.0f);

    data[0] = 0x00;
    data[1] = (rawRpm >> 8) & 0xFF;
    data[2] = rawRpm & 0xFF;

    canTransmit(0, 0x308, false, 8, data);
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
    (void)id;
    (void)data;
    (void)dlc;
}
