#include "pch.h"
#include "colt_can.h"

#include "can_msg_tx.h"
#include "can_category.h"
#include "sensor.h"

static uint32_t last14msTx = 0;
static constexpr size_t COLT_CAN_BUS = 0;

static int getCurrentRpm() {
	return (int)Sensor::getOrZero(SensorType::Rpm);
}

static bool getBatteryWarning() {
	const float vbatt = Sensor::getOrZero(SensorType::BatteryVoltage);
	return vbatt > 1.0f && vbatt < 11.8f;
}

static bool getMilWarning() {
	return false;
}

static bool getOilWarning() {
	return false;
}

static uint16_t encodeClusterRpm308(int rpm) {
	if (rpm < 0) rpm = 0;
	if (rpm > 8000) rpm = 8000;
	return (uint16_t)((rpm * 1024) / 1000);
}

static void sendCanFrame(uint32_t id, const uint8_t* data, uint8_t dlc) {
	CanTxMessage msg(CanCategory::NBC, id, dlc, COLT_CAN_BUS, false);

	for (uint8_t i = 0; i < dlc && i < 8; i++) {
		msg[i] = data[i];
	}
}

static void buildFrame308(uint8_t* d) {
	memset(d, 0, 8);

	const int rpm = getCurrentRpm();
	const uint16_t rawRpm = encodeClusterRpm308(rpm);

	d[0] = 0x00;
	d[1] = (rawRpm >> 8) & 0xFF;
	d[2] = rawRpm & 0xFF;

	d[3] = 0x00;
	if (getMilWarning()) {
		d[3] |= 0x02;
	}
	if (getOilWarning()) {
		d[3] |= 0x04;
	}

	d[4] = 0x00;
	if (getBatteryWarning()) {
		d[4] |= 0x10;
	}

	d[5] = 0x00;
	d[6] = 0x80;
	d[7] = 0x00;
}

void initColtCan() {
	last14msTx = 0;
}

void processColtCanTx() {
	const uint32_t nowMs = getTimeNowMs();

	if ((nowMs - last14msTx) >= 14) {
		last14msTx = nowMs;

		uint8_t data[8];
		buildFrame308(data);
		sendCanFrame(0x308, data, 8);
	}
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)id;
	(void)data;
	(void)dlc;
}