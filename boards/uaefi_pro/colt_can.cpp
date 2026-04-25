#include "pch.h"
#include "colt_can.h"

#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
#include "can.h"
#include "can_msg_tx.h"
#include "sensor.h"

namespace {

static constexpr size_t COLT_CAN_BUS = 0;

struct ColtRuntimeState {
	bool brakePressed = false;
	bool acRequest = false;
	bool ascIntervention = false;
	uint8_t meterState = 0;
};

static ColtRuntimeState g_coltCanState;

static uint8_t clampToU8(int value) {
	if (value < 0) {
		return 0;
	}

	if (value > 0xFF) {
		return 0xFF;
	}

	return static_cast<uint8_t>(value);
}

static float clampPercent(float value) {
	if (value < 0.0f) {
		return 0.0f;
	}

	if (value > 100.0f) {
		return 100.0f;
	}

	return value;
}

static int getCurrentRpm() {
	return static_cast<int>(Sensor::getOrZero(SensorType::Rpm));
}

static float getThrottlePercent() {
	return clampPercent(Sensor::getOrZero(SensorType::Tps1));
}

static bool isEngineRunning() {
	return getCurrentRpm() > 400;
}

static bool hasBatteryWarning() {
	const float vbatt = Sensor::getOrZero(SensorType::BatteryVoltage);
	return vbatt > 1.0f && vbatt < 11.8f;
}

static bool hasMilWarning() {
	return false;
}

static bool oilStatusBit() {
	return true;
}

static bool isAcRequested() {
	return g_coltCanState.acRequest;
}

static uint16_t encodeColtDashRpm(float rpm) {
	if (rpm < 0) {
		rpm = 0;
	}

	uint32_t raw = static_cast<uint32_t>((rpm * 1024.0f / 1000.0f) + 0.5f);

	if (raw > 0xFFFF) {
		raw = 0xFFFF;
	}

	return static_cast<uint16_t>(raw);
}

static void sendFrame0C0() {
	const int rpm = getCurrentRpm();

	CanTxMessage msg(CanCategory::NBC, 0x0C0, 8, COLT_CAN_BUS);
	msg[0] = clampToU8((rpm * 255) / 12000);
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = clampToU8((rpm * 255) / 8000);
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame210() {
	const uint8_t scaledTps = clampToU8(static_cast<int>((getThrottlePercent() * 250.0f) / 100.0f));

	CanTxMessage msg(CanCategory::NBC, 0x210, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x00;
	msg[2] = scaledTps;
	msg[3] = isEngineRunning() ? 0x40 : 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0xFF;
}

static uint8_t encode212Byte5(float tpsPercent) {
	tpsPercent = clampPercent(tpsPercent);

	if (tpsPercent < 5.0f) {
		return 0xE0;
	}

	if (tpsPercent < 25.0f) {
		return 0xE9;
	}

	if (tpsPercent < 60.0f) {
		return 0xEE;
	}

	if (tpsPercent < 85.0f) {
		return 0xF7;
	}

	return 0xFA;
}

static void sendFrame212() {
	CanTxMessage msg(CanCategory::NBC, 0x212, 8, COLT_CAN_BUS);
	msg[0] = isEngineRunning() ? 0x03 : 0x05;
	msg[1] = isEngineRunning() ? 0x92 : 0x66;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x68;
	msg[5] = encode212Byte5(getThrottlePercent());
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame308() {
	const uint16_t rawRpm = encodeColtDashRpm(Sensor::getOrZero(SensorType::Rpm));

	CanTxMessage msg(CanCategory::NBC, 0x308, 8, COLT_CAN_BUS);

	msg[0] = 0x00;
	msg[1] = (rawRpm >> 8) & 0xFF;
	msg[2] = rawRpm & 0xFF;
	if (isEngineRunning()) {
		msg[3] = 0x00;
		msg[4] = 0x00;
		msg[5] = isAcRequested() ? 0x45 : 0x52;
		msg[6] = 0xFF;
	} else {
		uint8_t dashStatus = 0x04;
		if (hasMilWarning()) {
			dashStatus |= 0x02;
		}

		if (!oilStatusBit()) {
			dashStatus &= static_cast<uint8_t>(~0x04);
		}

		msg[3] = dashStatus;
		msg[4] = hasBatteryWarning() ? 0x10 : 0x00;
		msg[5] = 0x33;
	}
	msg[6] = 0xFF;
	msg[7] = 0x00;
}

static void sendFrame408() {
	CanTxMessage msg(CanCategory::NBC, 0x408, 8, COLT_CAN_BUS);
	if (isEngineRunning() && isAcRequested()) {
		msg[0] = 0x0F;
		msg[1] = 0x00;
		msg[2] = 0x6F;
		msg[3] = 0xFF;
		msg[4] = 0xEC;
		msg[5] = 0x34;
		msg[6] = 0xF0;
		msg[7] = 0x00;
		return;
	}

	msg[0] = 0x0F;
	msg[1] = 0x00;
	msg[2] = isEngineRunning() ? 0x63 : 0x64;
	msg[3] = 0xFF;
	msg[4] = 0xFE;
	msg[5] = g_coltCanState.brakePressed ? 0xC4 : 0xC3;
	msg[6] = g_coltCanState.brakePressed ? 0xF0 : 0x4F;
	msg[7] = 0x00;
}

static void sendFrame412() {
	CanTxMessage msg(CanCategory::NBC, 0x412, 8, COLT_CAN_BUS);
	msg[0] = g_coltCanState.brakePressed ? 0x58 : 0x5A;
	msg[1] = 0x00;
	msg[2] = 0x05;
	msg[3] = 0xD7;
	msg[4] = 0x8C;
	msg[5] = g_coltCanState.brakePressed ? 0x5C : 0x5D;
	msg[6] = 0x01;
	msg[7] = 0xFF;
}

static void sendFrame416() {
	const int rpm = getCurrentRpm();

	CanTxMessage msg(CanCategory::NBC, 0x416, 8, COLT_CAN_BUS);
	if (!isEngineRunning()) {
		msg[0] = g_coltCanState.brakePressed ? 0x72 : 0x74;
	} else if (rpm < 900) {
		msg[0] = 0x8D;
	} else if (rpm < 1100) {
		msg[0] = 0x8E;
	} else if (rpm < 1500) {
		msg[0] = 0x8F;
	} else {
		msg[0] = 0x90;
	}
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame423() {
	const int rpm = getCurrentRpm();

	CanTxMessage msg(CanCategory::NBC, 0x423, 6, COLT_CAN_BUS);
	if (!isEngineRunning()) {
		msg[0] = 0x01;
	} else if (rpm > 1200) {
		msg[0] = 0x07;
	} else {
		msg[0] = 0x03;
	}
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = isAcRequested() ? 0x09 : 0x08;
	msg[4] = 0x2E;
	msg[5] = 0xBC;
}

static void sendFrame443() {
	CanTxMessage msg(CanCategory::NBC, 0x443, 6, COLT_CAN_BUS);
	msg[0] = isAcRequested() ? 0x01 : 0x00;
	msg[1] = 0x02;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
}

static void sendFrame584() {
	CanTxMessage msg(CanCategory::NBC, 0x584, 1, COLT_CAN_BUS);
	msg[0] = 0xC0;
}

} // namespace
#endif // !EFI_BOOTLOADER && EFI_CAN_SUPPORT

void initColtCan() {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	g_coltCanState = {};
#endif
}

void processColtCanTx(CanCycle cycle) {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	if (cycle.isInterval(CI::_10ms)) {
		sendFrame0C0();
		sendFrame308();
	}

	if (cycle.isInterval(CI::_20ms)) {
		sendFrame210();
		sendFrame212();

		static bool sendKeepaliveThisTick = false;
		sendKeepaliveThisTick = !sendKeepaliveThisTick;

		if (sendKeepaliveThisTick) {
			sendFrame584();
		}
	}

	if (cycle.isInterval(CI::_100ms)) {
		sendFrame423();
		sendFrame443();
		sendFrame408();
		sendFrame412();
		sendFrame416();
	}
#else
	UNUSED(cycle);
#endif
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	switch (id) {
		case 0x200:
			if (dlc > 1) {
				g_coltCanState.brakePressed = (data[1] & 0x05) == 0x05;
			}
			break;

		case 0x300:
			if (dlc > 0) {
				g_coltCanState.ascIntervention = (data[0] & 0x80) != 0;
			}
			break;

		case 0x412:
			if (dlc > 0) {
				g_coltCanState.meterState = data[0];
			}
			break;

		case 0x443:
			if (dlc > 0) {
				g_coltCanState.acRequest = (data[0] & 0x01) != 0;
			}
			break;

		default:
			break;
	}
#else
	UNUSED(id);
	UNUSED(data);
	UNUSED(dlc);
#endif
}
