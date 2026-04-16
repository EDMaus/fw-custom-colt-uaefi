#include "pch.h"
#include "colt_can.h"

#include "can_msg_tx.h"
#include "can_category.h"
#include "sensor.h"

// ============================================================
// Colt CZT CAN v1
// Native primary cluster/body scene for rusEFI
//
// Hard-proven core frames:
//   0x308 = RPM + MIL/OIL/BATT
//   0x423 = body/status/light scene
//   0x443 = AC companion/status
//
// This file intentionally prioritizes proven behavior over
// speculative extra frames.
// ============================================================

static constexpr size_t COLT_CAN_BUS = 0;

// -----------------------------
// Timing
// -----------------------------
static uint32_t last14msTx = 0;
static uint32_t last28msTx = 0;

// -----------------------------
// Runtime state
// -----------------------------
struct ColtRuntime {
	bool brakePressed = false;
	bool parkingBrake = false;
	bool clutchPressed = false;

	// AC request can later come from real input logic.
	// For now it can still be updated by RX or left false.
	bool acRequest = false;

	// TODO later hook these to real body inputs if available
	bool doorOpen = false;
	uint8_t lightMode = 0;   // 0=off,1=auto,2=parking,3=dim,4=high

	float vehicleSpeedKph = 0.0f;
	float coolantTempC = 0.0f;
	float rpm = 0.0f;
	float tpsPct = 0.0f;
};

static ColtRuntime g_colt;

// -----------------------------
// ECU scene state
// -----------------------------
enum class ColtEcuState : uint8_t {
	KOEO = 0,      // key on engine off
	CRANKING = 1,
	RUNNING = 2,
};

static ColtEcuState getColtState() {
	const float rpm = Sensor::getOrZero(SensorType::Rpm);

	if (rpm > 400.0f) {
		return ColtEcuState::RUNNING;
	}

	// Cranking window heuristic:
	// if some RPM exists but engine is not yet considered running
	if (rpm > 40.0f) {
		return ColtEcuState::CRANKING;
	}

	return ColtEcuState::KOEO;
}

// -----------------------------
// Helpers
// -----------------------------
static uint8_t clampToU8(int v) {
	if (v < 0) return 0;
	if (v > 255) return 255;
	return static_cast<uint8_t>(v);
}

static float clampPct(float v) {
	if (v < 0.0f) return 0.0f;
	if (v > 100.0f) return 100.0f;
	return v;
}

static int getCurrentRpm() {
	return static_cast<int>(Sensor::getOrZero(SensorType::Rpm));
}

static int getCoolantTempC() {
	return static_cast<int>(Sensor::getOrZero(SensorType::Clt));
}

static int getVehicleSpeedKph() {
	return static_cast<int>(Sensor::getOrZero(SensorType::VehicleSpeed));
}

static float getTpsPercent() {
	return clampPct(Sensor::getOrZero(SensorType::Tps1));
}

static bool getAcRequest() {
	return g_colt.acRequest;
}

static bool getBatteryWarning() {
	const float vbatt = Sensor::getOrZero(SensorType::BatteryVoltage);
	return vbatt > 1.0f && vbatt < 11.8f;
}

static bool getMilWarning() {
	// TODO: replace with real MIL/CEL status when available
	return false;
}

static bool getOilWarning() {
	// TODO: replace with real oil pressure switch / logic when available
	return false;
}

// Bench-proven Colt cluster RPM encoding:
// raw = rpm * 1024 / 1000
static uint16_t encodeClusterRpm308(int rpm) {
	if (rpm < 0) rpm = 0;
	if (rpm > 8000) rpm = 8000;
	return static_cast<uint16_t>((rpm * 1024) / 1000);
}

static void sendCanFrame(uint32_t id, const uint8_t* data, uint8_t dlc) {
	CanTxMessage msg(CanCategory::NBC, id, dlc, COLT_CAN_BUS, false);

	for (uint8_t i = 0; i < dlc && i < 8; i++) {
		msg[i] = data[i];
	}

	// TX occurs through message object lifetime
}

// -----------------------------
// Frame builders
// -----------------------------

// ------------------------------------------------------------
// 0x308
// Hard proven cluster frame:
//   byte1:byte2 = RPM raw (big-endian)
//   byte3 bit1  = MIL
//   byte3 bit2  = OIL
//   byte4 bit4  = BATT
//   byte6       = 0x80
// ------------------------------------------------------------
static void buildFrame308(uint8_t* d) {
	memset(d, 0, 8);

	const int rpm = getCurrentRpm();
	const uint16_t rawRpm = encodeClusterRpm308(rpm);

	d[0] = 0x00;
	d[1] = static_cast<uint8_t>((rawRpm >> 8) & 0xFF);
	d[2] = static_cast<uint8_t>(rawRpm & 0xFF);

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

// ------------------------------------------------------------
// 0x423
// Proven body/status scene frame.
//
// Bench-proven baseline:
//   03 00 00 09 2E BC
//
// Bench-proven door effect:
//   byte2 = 0x02 => door open
//
// Bench-proven light modes from ESP32 reference:
//   l0 => byte0=0x03 byte1=0x00
//   l1 => byte0=0x07 byte1=0x00
//   l2/l3/l4 => byte0=0x07 byte1=0x04
//
// We keep the proven base stable and only vary the parts
// already validated.
// ------------------------------------------------------------
static void buildFrame423(uint8_t* d) {
	memset(d, 0, 6);

	// Proven stable baseline
	d[0] = 0x03;
	d[1] = 0x00;
	d[2] = g_colt.doorOpen ? 0x02 : 0x00;
	d[3] = 0x09;
	d[4] = 0x2E;
	d[5] = 0xBC;

	switch (g_colt.lightMode) {
		case 0: // off
			d[0] = 0x03;
			d[1] = 0x00;
			break;

		case 1: // auto
			d[0] = 0x07;
			d[1] = 0x00;
			break;

		case 2: // parking
		case 3: // dim
		case 4: // high
			d[0] = 0x07;
			d[1] = 0x04;
			break;

		default:
			d[0] = 0x03;
			d[1] = 0x00;
			break;
	}
}

// ------------------------------------------------------------
// 0x443
// Proven AC companion/status frame.
//
// Bench-proven:
//   AC off => 00 02 00 00 00 00
//   AC on  => 01 02 00 00 00 00
// ------------------------------------------------------------
static void buildFrame443(uint8_t* d) {
	memset(d, 0, 6);

	d[0] = getAcRequest() ? 0x01 : 0x00;
	d[1] = 0x02;
	d[2] = 0x00;
	d[3] = 0x00;
	d[4] = 0x00;
	d[5] = 0x00;
}

// -----------------------------
// Public API
// -----------------------------
void initColtCan() {
	last14msTx = 0;
	last28msTx = 0;
}

void processColtCanTx() {
	const uint32_t nowMs = getTimeNowMs();

	g_colt.rpm = static_cast<float>(getCurrentRpm());
	g_colt.coolantTempC = static_cast<float>(getCoolantTempC());
	g_colt.vehicleSpeedKph = static_cast<float>(getVehicleSpeedKph());
	g_colt.tpsPct = getTpsPercent();

	const ColtEcuState ecuState = getColtState();
	(void)ecuState;
	// State is available for future refinement.
	// For now, proven baseline behavior is prioritized.

	// 0x308 + 0x443 at ~14ms
	if ((nowMs - last14msTx) >= 14) {
		last14msTx = nowMs;

		uint8_t data8[8];
		uint8_t data6[6];

		buildFrame308(data8);
		sendCanFrame(0x308, data8, 8);

		buildFrame443(data6);
		sendCanFrame(0x443, data6, 6);
	}

	// 0x423 at ~28ms (bench reference was ~27.5ms)
	if ((nowMs - last28msTx) >= 28) {
		last28msTx = nowMs;

		uint8_t data6[6];
		buildFrame423(data6);
		sendCanFrame(0x423, data6, 6);
	}
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
	switch (id) {
		case 0x200:
			// Existing heuristic from your earlier work
			if (dlc > 1) {
				g_colt.brakePressed = ((data[1] & 0x05) == 0x05);
			}
			break;

		case 0x443:
			// If another module still broadcasts AC state on the car,
			// keep this for now. Once rusEFI is sole owner of the frame,
			// you can replace this with a real AC request input source.
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