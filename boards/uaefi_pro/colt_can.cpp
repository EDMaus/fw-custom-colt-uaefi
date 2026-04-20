#include "pch.h"
#include "colt_can.h"

#ifdef EFI_BOOTLOADER

void initColtCan() {
}

void processColtCanTx() {
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)id;
	(void)data;
	(void)dlc;
}

#else

#include "can_msg_tx.h"

static uint32_t lastTxTime = 0;

void initColtCan() {
}

void processColtCanTx() {
	uint32_t now = getTimeNowUs();

	if (now - lastTxTime < 10000) {
		return;
	}

	lastTxTime = now;

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	uint16_t rawRpm = (uint16_t)(rpm * 1024.0f / 1000.0f);

	CanTxMessage msg(CanCategory::NBC, 0x308, 8, DEFAULT_BUS_INDEX);

	msg[0] = 0x00;
	msg[1] = (rawRpm >> 8) & 0xFF;
	msg[2] = rawRpm & 0xFF;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)id;
	(void)data;
	(void)dlc;
}

#endif
