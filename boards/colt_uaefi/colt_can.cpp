// boards/colt_uaefi/colt_can.cpp

#include "pch.h"
#include "colt_can.h"

// ============================================================================
// LET OP
// ----------------------------------------------------------------------------
// Dit is een SKELETON.
// De exacte rusEFI helper/API namen voor CAN TX en sensor/state access kunnen
// per versie verschillen. Dus:
// - CAN TX call mogelijk aanpassen
// - sensor access mogelijk aanpassen
// - hook waar processColtCanTx() wordt aangeroepen mogelijk aanpassen
// ============================================================================

// ----------------------------------------------------------------------------
// Config / timing
// ----------------------------------------------------------------------------

static uint32_t lastFastTx = 0;
static uint32_t lastSlowTx = 0;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static uint8_t clampToU8(int v) {
	if (v < 0) {
		return 0;
	}

	if (v > 255) {
		return 255;
	}

	return (uint8_t)v;
}

static uint16_t clampToU16(int v) {
	if (v < 0) {
		return 0;
	}

	if (v > 65535) {
		return 65535;
	}

	return (uint16_t)v;
}

// ----------------------------------------------------------------------------
// PLACEHOLDER ECU data access
// ----------------------------------------------------------------------------
// Vervang deze helpers later met echte rusEFI state/sensor calls.
// Bijvoorbeeld via Sensor::getOrZero(...), engine->outputChannels, etc.
// ----------------------------------------------------------------------------

static int getCurrentRpm() {
	// TODO: vervangen met echte rusEFI RPM bron
	return 0;
}

static int getCoolantTempC() {
	// TODO: vervangen met echte rusEFI CLT bron
	return 0;
}

static int getVehicleSpeedKph() {
	// TODO: vervangen met echte VSS bron
	return 0;
}

static bool getMilActive() {
	// TODO: vervangen met echte MIL/engine fault status
	return false;
}

static bool getOilWarningActive() {
	// TODO: vervangen met echte oil pressure / warning logica
	return false;
}

static bool getChargeWarningActive() {
	// TODO: vervangen met echte alternator / charging status
	return false;
}

// ----------------------------------------------------------------------------
// CAN TX helper
// ----------------------------------------------------------------------------
// Deze functie moet je aanpassen aan de exacte rusEFI CAN TX API in jouw tree.
// Bijvoorbeeld via een canTransmit/canWrite/canSend helper.
// ----------------------------------------------------------------------------

static void sendCanFrame(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)id;
	(void)data;
	(void)dlc;

	// TODO:
	// vervang deze stub met de juiste rusEFI CAN TX call
	//
	// Bijvoorbeeld conceptueel:
	// canTransmit(CanBus::Bus1, id, false, data, dlc);
}

// ----------------------------------------------------------------------------
// Colt cluster frames - placeholders
// ----------------------------------------------------------------------------

static void sendColtFrame_RpmCoolant() {
	// TODO: vervang met echte Colt cluster CAN ID
	const uint32_t canId = 0x316;

	uint8_t data[8] = { 0 };

	const int rpm = getCurrentRpm();
	const int clt = getCoolantTempC();

	// TODO:
	// vervang packing met echte Colt scaling/endianness
	const uint16_t rpmPacked = clampToU16(rpm);
	data[0] = (uint8_t)(rpmPacked & 0xFF);
	data[1] = (uint8_t)((rpmPacked >> 8) & 0xFF);

	// voorbeeld: offset temp indien nodig later aanpassen
	data[2] = clampToU8(clt + 40);

	sendCanFrame(canId, data, 8);
}

static void sendColtFrame_Speed() {
	// TODO: vervang met echte Colt cluster CAN ID
	const uint32_t canId = 0x300;

	uint8_t data[8] = { 0 };

	const int speedKph = getVehicleSpeedKph();

	// TODO:
	// vervang packing met echte Colt scaling
	const uint16_t speedPacked = clampToU16(speedKph);
	data[0] = (uint8_t)(speedPacked & 0xFF);
	data[1] = (uint8_t)((speedPacked >> 8) & 0xFF);

	sendCanFrame(canId, data, 8);
}

static void sendColtFrame_Warnings() {
	// TODO: vervang met echte Colt cluster CAN ID
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

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void initColtCan() {
	lastFastTx = 0;
	lastSlowTx = 0;
}

void processColtCanTx() {
	// TODO:
	// vervang getTimeNowMs() met de echte rusEFI tijdhelper indien nodig
	const uint32_t nowMs = getTimeNowMs();

	// snelle frames, bv elke 10 ms
	if ((nowMs - lastFastTx) >= 10) {
		lastFastTx = nowMs;

		sendColtFrame_RpmCoolant();
		sendColtFrame_Speed();
	}

	// langzamere statusframes, bv elke 100 ms
	if ((nowMs - lastSlowTx) >= 100) {
		lastSlowTx = nowMs;

		sendColtFrame_Warnings();
	}
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	(void)data;
	(void)dlc;

	switch (id) {
		// TODO: vervang met echte ETACS / cluster RX IDs als je die nodig hebt
		case 0x123:
			// parse incoming frame
			break;

		default:
			break;
	}
}