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

static int getCurrentRpm() {
	return static_cast<int>(Sensor::getOrZero(SensorType::Rpm));
}

static bool isEngineRunning() {
	return getCurrentRpm() > 400;
}

static bool isIgnitionOn() {
	return isIgnVoltage();
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
	CanTxMessage msg(CanCategory::NBC, 0x210, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = isEngineRunning() ? 0x40 : 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0xFF;
}

static void sendFrame212() {
	CanTxMessage msg(CanCategory::NBC, 0x212, 8, COLT_CAN_BUS);
	if (isEngineRunning()) {
		msg[0] = 0x03;
		msg[1] = 0xA1;
		msg[2] = 0x00;
		msg[3] = 0x00;
		msg[4] = 0x68;
		msg[5] = 0x12;
		msg[6] = 0x00;
		msg[7] = 0x00;
		return;
	}

	msg[0] = 0x05;
	msg[1] = 0x66;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x68;
	msg[5] = 0xEC;
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
		msg[5] = 0x53;
		msg[6] = 0xFF;
	} else {
		msg[0] = 0x00;
		msg[3] = 0x04;
		msg[4] = 0x00;
		msg[5] = 0x33;
	}
	msg[6] = 0xFF;
	msg[7] = 0x00;
}

static void sendFrame312() {
	CanTxMessage msg(CanCategory::NBC, 0x312, 8, COLT_CAN_BUS);

	if (isEngineRunning()) {
		msg[0] = 0x07;
		msg[1] = 0xED;
		msg[2] = 0x07;
		msg[3] = 0xED;
		msg[4] = 0x09;
		msg[5] = 0x58;
		msg[6] = 0x07;
		msg[7] = 0x9F;
		return;
	}

	msg[0] = 0x07;
	msg[1] = 0x6F;
	msg[2] = 0x07;
	msg[3] = 0x6F;
	msg[4] = 0x09;
	msg[5] = 0x00;
	msg[6] = 0x07;
	msg[7] = 0x8E;
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

bool isColtAcRequested() {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	return g_coltCanState.acRequest;
#else
	return false;
#endif
}

void processColtCanTx(CanCycle cycle) {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	if (!isIgnitionOn()) {
		return;
	}

	if (cycle.isInterval(CI::_10ms)) {
		sendFrame0C0();
	}

	if (cycle.isInterval(CI::_20ms)) {
		sendFrame210();
		sendFrame212();
		sendFrame308();
		sendFrame312();

		static bool send40msThisTick = false;
		send40msThisTick = !send40msThisTick;

		if (send40msThisTick) {
			sendFrame584();
		}
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
