#include "pch.h"
#include "colt_can.h"

#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
#include "can.h"
#include "can_msg_tx.h"
#include "sensor.h"

namespace {

static constexpr size_t COLT_CAN_BUS = 0;
static constexpr uint32_t COLT_MIL_BULB_CHECK_20MS_TICKS = 200; // 4 seconds
static constexpr uint32_t COLT_MIL_SELF_CHECK_SETTLE_20MS_TICKS = 250; // 5 seconds total
static constexpr uint32_t COLT_SRS_CLEAR_START_20MS_TICKS = 335; // ~6.7 seconds

struct ColtRuntimeState {
	bool brakePressed = false;
	bool acRequest = false;
	bool ascIntervention = false;
	uint8_t meterState = 0;
};

static ColtRuntimeState g_coltCanState;
static uint32_t g_ignitionOn20msTicks = 0;

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

static void sendFrame1E1Clear() {
	CanTxMessage msg(CanCategory::NBC, 0x1E1, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame210() {
	CanTxMessage msg(CanCategory::NBC, 0x210, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x00;
	// OEM key-on behavior shows only a very short "01" phase here.
	msg[2] = (!isEngineRunning() && g_ignitionOn20msTicks <= 1) ? 0x01 : 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0xFF;

	if (isEngineRunning()) {
		msg[3] = 0x40;
		msg[5] = 0x00;
	}
}

static void sendFrame212() {
	CanTxMessage msg(CanCategory::NBC, 0x212, 8, COLT_CAN_BUS);
	msg[0] = 0x05;
	msg[1] = (g_ignitionOn20msTicks <= 8) ? 0x3F : 0x37;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x68;
	if (!isEngineRunning()) {
		msg[5] = (g_ignitionOn20msTicks <= 8) ? 0xE0 : 0xDA;
	} else {
		const int rpm = getCurrentRpm();
		if (rpm >= 1500) {
			msg[5] = 0x2F;
		} else if (rpm >= 1200) {
			msg[5] = 0x31;
		} else if (rpm >= 1000) {
			msg[5] = 0x34;
		} else if (rpm >= 900) {
			msg[5] = 0x37;
		} else {
			msg[5] = 0x3A;
		}
	}
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame308() {
	const uint16_t rawRpm = encodeColtDashRpm(Sensor::getOrZero(SensorType::Rpm));

	CanTxMessage msg(CanCategory::NBC, 0x308, 8, COLT_CAN_BUS);

	// OEM key-on/running traces carry 0x80 in byte 0.
	msg[0] = 0x80;
	msg[1] = (rawRpm >> 8) & 0xFF;
	msg[2] = rawRpm & 0xFF;
	if (isEngineRunning()) {
		msg[3] = 0x00;
		msg[4] = 0x00;
		msg[5] = 0x53;
		msg[6] = 0xFF;
	} else {
		if (g_ignitionOn20msTicks <= COLT_MIL_BULB_CHECK_20MS_TICKS) {
			msg[3] = 0x06;
			msg[4] = 0x01;
		} else {
			msg[3] = 0x04;
			msg[4] = (g_ignitionOn20msTicks <= COLT_MIL_SELF_CHECK_SETTLE_20MS_TICKS) ? 0x01 : 0x00;
		}
		// OEM key-on reference shows 0x3E here.
		msg[5] = 0x3E;
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

static void sendFrame443() {
	CanTxMessage msg(CanCategory::NBC, 0x443, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	// OEM baseline is 0x11, with bit0 set when AC request is active (0x13).
	msg[1] = g_coltCanState.acRequest ? 0x13 : 0x11;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame608() {
	CanTxMessage msg(CanCategory::NBC, 0x608, 8, COLT_CAN_BUS);

	if (isEngineRunning()) {
		msg[0] = 0x66;
		msg[1] = 0x00;
		msg[2] = 0x18;
		msg[3] = 0xC3;
		msg[4] = 0xFF;
		msg[5] = 0x00;
		msg[6] = 0x65;
		msg[7] = 0x00;
		return;
	}

	msg[0] = 0x34;
	msg[1] = 0x00;
	msg[2] = 0x18;
	msg[3] = 0xC3;
	msg[4] = 0xFF;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

} // namespace
#endif // !EFI_BOOTLOADER && EFI_CAN_SUPPORT

void initColtCan() {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	g_coltCanState = {};
	g_ignitionOn20msTicks = 0;
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
		g_ignitionOn20msTicks = 0;
		return;
	}

	if (cycle.isInterval(CI::_20ms)) {
		g_ignitionOn20msTicks++;
		const bool shouldHoldSrsClear = isEngineRunning() || (g_ignitionOn20msTicks >= COLT_SRS_CLEAR_START_20MS_TICKS);
		if (shouldHoldSrsClear) {
			sendFrame1E1Clear();
		}
		sendFrame210();
		sendFrame212();
		sendFrame308();
		sendFrame312();
		sendFrame443();

	}

	if (cycle.isInterval(CI::_100ms)) {
		sendFrame608();
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
