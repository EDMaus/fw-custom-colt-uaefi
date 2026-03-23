#include "pch.h"
#include "colt_can.h"

#include "can_msg_tx.h"
#include "can_category.h"
#include "sensor.h"

static uint32_t lastFastTx = 0;
static uint32_t lastSlowTx = 0;

static constexpr size_t COLT_CAN_BUS = 0;

static uint8_t clampToU8(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return (uint8_t)v;
}

static uint16_t clampToU16(int v) {
	if (v < 0) return 0;
	if (v > 65535) return 65535;
	return (uint16_t)v;
}

static int getCurrentRpm() {
	return (int)Sensor::getOrZero(SensorType::Rpm);
}

static int getCoolantTempC() {
	return (int)Sensor::getOrZero(SensorType::Clt);
}

static int getVehicleSpeedKph() {
	return (int)Sensor::getOrZero(SensorType::VehicleSpeed);
}

static bool getMilActive() {
	return false; // TODO
}

static bool getOilWarningActive() {
	return false; // TODO
}

static bool getChargeWarningActive() {
	return false; // TODO
}

static void sendCanFrame(uint32_t id, const uint8_t* data, uint8_t dlc) {
	CanTxMessage msg(CanCategory::NBC, id, dlc, COLT_CAN_BUS, false);

	for (uint8_t i = 0; i < dlc && i < 8; i++) {
		msg[i] = data[i];
	}
}

// Candidate: 0x312 lijkt in jouw logs rijk dynamisch gauge-data te bevatten
static void sendColtFrame_RpmCoolant_312() {
	const uint32_t canId = 0x312;
	uint8_t data[8] = { 0 };

	const int rpm = getCurrentRpm();
	const int clt = getCoolantTempC();

	// TEST-only startpunt:
	// byte0-1 = rpm raw LE
	// byte2   = clt + 40
	const uint16_t rpmPacked = clampToU16(rpm);
	data[0] = (uint8_t)(rpmPacked & 0xFF);
	data[1] = (uint8_t)((rpmPacked >> 8) & 0xFF);
	data[2] = clampToU8(clt + 40);

	sendCanFrame(canId, data, 8);
}

// Candidate: laat 0x300 bestaan als snelheids-testframe
static void sendColtFrame_Speed_300() {
	const uint32_t canId = 0x300;
	uint8_t data[8] = { 0 };

	const int speedKph = getVehicleSpeedKph();
	const uint16_t speedPacked = clampToU16(speedKph);

	data[0] = (uint8_t)(speedPacked & 0xFF);
	data[1] = (uint8_t)((speedPacked >> 8) & 0xFF);

	sendCanFrame(canId, data, 8);
}

// Candidate: 0x200 ook testen als speed-gerelateerd frame
static void sendColtFrame_Speed_200() {
	const uint32_t canId = 0x200;
	uint8_t data[8] = { 0 };

	const int speedKph = getVehicleSpeedKph();

	// TEST-only: alleen byte1 vullen, omdat jouw log daar lineaire variatie liet zien
	data[1] = clampToU8(speedKph);

	sendCanFrame(canId, data, 8);
}

// 0x308 als status/warning kandidaat behouden
static void sendColtFrame_Warnings_308() {
	const uint32_t canId = 0x308;
	uint8_t data[8] = { 0 };

	if (getMilActive()) {
		data[0] |= 0x01;
	}

	if (getOilWarningActive()) {
		data[0] |= 0x02;
	}

	if (getChargeWarningActive()) {
		data[0] |= 0x04;
	}

	sendCanFrame(canId, data, 8);
}

void initColtCan() {
	lastFastTx = 0;
	lastSlowTx = 0;
}

void processColtCanTx() {
	const uint32_t nowMs = getTimeNowMs();

	if ((nowMs - lastFastTx) >= 10) {
		lastFastTx = nowMs;

		sendColtFrame_RpmCoolant_312();
		sendColtFrame_Speed_300();
		sendColtFrame_Speed_200();
	}

	if ((nowMs - lastSlowTx) >= 100) {
		lastSlowTx = nowMs;

		sendColtFrame_Warnings_308();
	}
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)id;
	(void)data;
	(void)dlc;
	// later VSS RX hier inbouwen zodra juiste ABS/ETACS ID bevestigd is
}