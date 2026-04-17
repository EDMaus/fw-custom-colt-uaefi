#include "pch.h"
#include "colt_can.h"

#include "can_msg_tx.h"
#include "can_category.h"
#include "sensor.h"

static uint32_t last10msTx = 0;
static uint32_t last14msTx = 0;
static uint32_t last20msTx = 0;
static uint32_t last100msTx = 0;

static constexpr size_t COLT_CAN_BUS = 0;

// ----------------------------------------------------
// Runtime RX state
// ----------------------------------------------------

struct ColtRuntime {
	bool brakePressed = false;
	bool parkingBrake = false;
	bool clutchPressed = false;   // TODO echte bron
	bool acRequest = false;

	float vehicleSpeedKph = 0.0f;
	float coolantTempC = 0.0f;
	float rpm = 0.0f;
	float tpsPct = 0.0f;
};

static ColtRuntime g_colt;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static uint8_t clampToU8(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return (uint8_t)v;
}

static float clampPct(float v) {
	if (v < 0.0f) return 0.0f;
	if (v > 100.0f) return 100.0f;
	return v;
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

static float getTpsPercent() {
	return clampPct(Sensor::getOrZero(SensorType::Tps1));
}

static bool getEngineRunning() {
	return getCurrentRpm() > 400;
}

static bool getAcRequest() {
	return g_colt.acRequest;
}

static bool getBatteryWarning() {
	const float vbatt = Sensor::getOrZero(SensorType::BatteryVoltage);
	return vbatt > 1.0f && vbatt < 11.8f;
}

static bool getMilWarning() {
	// TODO later koppelen aan echte MIL / CEL status
	return false;
}

static bool getOilWarning() {
	// TODO later koppelen aan echte oliedruk/engine protect logica
	return false;
}

// ----------------------------------------------------
// Proven Colt cluster RPM encoding for 0x308
// raw = rpm * 1024 / 1000
// ----------------------------------------------------
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

	// Constructor/object lifetime doet de TX
}

// ----------------------------------------------------
// Encoders based on logs
// ----------------------------------------------------

static uint8_t scaleTpsTo210(float tpsPercent) {
	tpsPercent = clampPct(tpsPercent);
	return clampToU8((int)((tpsPercent * 250.0f) / 100.0f));   // 0x00..0xFA
}

static uint8_t scaleTpsTo212Byte5(float tpsPercent) {
	tpsPercent = clampPct(tpsPercent);

	if (tpsPercent < 5.0f) {
		return 0xE0;
	} else if (tpsPercent < 25.0f) {
		return 0xE9;
	} else if (tpsPercent < 60.0f) {
		return 0xEE;
	} else if (tpsPercent < 85.0f) {
		return 0xF7;
	} else {
		return 0xFA;
	}
}

static uint8_t encode0C0Byte0(int rpm) {
	if (rpm < 0) rpm = 0;
	if (rpm > 12000) rpm = 12000;
	return clampToU8((rpm * 255) / 12000);
}

static uint8_t encode0C0Byte4(int rpm) {
	if (rpm < 0) rpm = 0;
	if (rpm > 8000) rpm = 8000;
	return clampToU8((rpm * 255) / 8000);
}

// ----------------------------------------------------
// Frame builders
// ----------------------------------------------------

// 0x0C0: strongest RPM-related candidate from logs
static void buildFrame0C0(uint8_t* d) {
	memset(d, 0, 8);

	const int rpm = getCurrentRpm();

	d[0] = encode0C0Byte0(rpm);
	d[4] = encode0C0Byte4(rpm);
}

// 0x210: load/throttle-like, byte2
static void buildFrame210(uint8_t* d) {
	memset(d, 0, 8);

	d[7] = 0xFF;
	d[2] = scaleTpsTo210(getTpsPercent());
}

// 0x212: torque/load companion, byte5
static void buildFrame212(uint8_t* d) {
	memset(d, 0, 8);

	d[0] = 0x1C;
	d[5] = scaleTpsTo212Byte5(getTpsPercent());
}

// 0x308: bewezen cluster frame
// byte1-2 big-endian = RPM
// byte3 bit1 = MIL
// byte3 bit2 = oil lamp
// byte4 bit4 = battery lamp
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

// 0x408: state frame from logs
static void buildFrame408(uint8_t* d) {
	memset(d, 0, 8);

	d[0] = getEngineRunning() ? 0x11 : 0x10;
	d[1] = 0x00;
	d[2] = 0x69;
	d[3] = 0xFF;
	d[4] = getAcRequest() ? 0xA1 : 0x9E;
	d[5] = 0xC3;
	d[6] = 0x4F;
	d[7] = 0x00;
}

// 0x416: state byte around 0x75..0x7A
static void buildFrame416(uint8_t* d) {
	memset(d, 0, 8);

	const int rpm = getCurrentRpm();

	if (rpm < 400) {
		d[0] = 0x75;
	} else if (rpm < 900) {
		d[0] = 0x77;
	} else if (rpm < 1500) {
		d[0] = 0x78;
	} else if (rpm < 2500) {
		d[0] = 0x79;
	} else {
		d[0] = 0x7A;
	}
}

// 0x423: mode/state
static void buildFrame423(uint8_t* d) {
	memset(d, 0, 6);

	d[0] = getEngineRunning() ? 0x03 : 0x01;
	d[1] = 0x00;
	d[2] = 0x00;
	d[3] = getAcRequest() ? 0x09 : 0x08;
	d[4] = 0x2E;
	d[5] = 0xBC;
}

// 0x584: keepalive
static void buildFrame584(uint8_t* d) {
	d[0] = 0xC0;
}

// ----------------------------------------------------
// Public API
// ----------------------------------------------------

void initColtCan() {
	last10msTx = 0;
	last14msTx = 0;
	last20msTx = 0;
	last100msTx = 0;
}

void processColtCanTx() {
	const uint32_t nowMs = getTimeNowMs();

	g_colt.rpm = (float)getCurrentRpm();
	g_colt.coolantTempC = (float)getCoolantTempC();
	g_colt.vehicleSpeedKph = (float)getVehicleSpeedKph();
	g_colt.tpsPct = getTpsPercent();

	if ((nowMs - last10msTx) >= 10) {
		last10msTx = nowMs;

		uint8_t data[8];

		buildFrame0C0(data);
		sendCanFrame(0x0C0, data, 8);
	}

	if ((nowMs - last14msTx) >= 14) {
		last14msTx = nowMs;

		uint8_t data[8];

		buildFrame308(data);
		sendCanFrame(0x308, data, 8);
	}

	if ((nowMs - last20msTx) >= 20) {
		last20msTx = nowMs;

		uint8_t data[8];

		buildFrame210(data);
		sendCanFrame(0x210, data, 8);

		buildFrame212(data);
		sendCanFrame(0x212, data, 8);

		//buildFrame312(data);
		//sendCanFrame(0x312, data, 8);
	}

	if ((nowMs - last100msTx) >= 100) {
		last100msTx = nowMs;

		uint8_t data8[8];
		uint8_t data6[6];
		uint8_t data1[1];

		buildFrame408(data8);
		sendCanFrame(0x408, data8, 8);

		buildFrame416(data8);
		sendCanFrame(0x416, data8, 8);

		buildFrame423(data6);
		sendCanFrame(0x423, data6, 6);

		buildFrame584(data1);
		sendCanFrame(0x584, data1, 1);
	}
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	switch (id) {
		case 0x200:
			if (dlc > 1) {
				g_colt.brakePressed = ((data[1] & 0x05) == 0x05);
			}
			break;

		case 0x443:
			if (dlc > 0) {
				g_colt.acRequest = (data[0] & 0x01) != 0;
			}
			break;

		case 0x300:
			// ASC -> engine
			break;

		case 0x412:
			// meter -> engine
			break;

		default:
			break;
	}
}