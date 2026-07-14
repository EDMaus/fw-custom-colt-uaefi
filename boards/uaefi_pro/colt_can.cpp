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
static constexpr uint32_t COLT_1E1_CLEAR_BURST_5MS_TICKS = 200; // 1000ms
static constexpr uint32_t COLT_STARTUP_INIT_REPEAT_MS = 30000;
static constexpr uint32_t COLT_STARTUP_INIT_REPEAT_20MS_TICKS = COLT_STARTUP_INIT_REPEAT_MS / 20;
static constexpr uint32_t COLT_2F1_CLEAR_COOLDOWN_5MS_TICKS = 200; // 1000ms

struct ColtRuntimeState {
	bool brakePressed = false;
	bool acRequest = false;
	bool ascIntervention = false;
	uint8_t meterState = 0;
};

static ColtRuntimeState g_coltCanState;
static uint32_t g_ignitionOn20msTicks = 0;
static uint32_t g_1e1ClearBurst5msTicks = 0;
static uint32_t g_2f1ClearCooldown5msTicks = 0;
static uint32_t g_startupInitRepeat20msTicks = 0;
static uint8_t g_startupInitSequence5msTick = 0;
static bool g_startupInitSequenceSent = false;
static bool g_startupInitSequenceRefresh = false;
static bool g_wasIgnitionOn = false;

static int getCurrentRpm() {
	return static_cast<int>(Sensor::getOrZero(SensorType::Rpm));
}

static bool isEngineRunning() {
	return getCurrentRpm() > 400;
}

static bool isColtFanTestHot() {
	return Sensor::getOrZero(SensorType::Clt) >= 95.0f;
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

static void sendFrame101Primary() {
	CanTxMessage msg(CanCategory::NBC, 0x101, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x9A;
	msg[2] = 0x78;
	msg[3] = 0x07;
	msg[4] = 0x87;
	msg[5] = 0x5E;
	msg[6] = 0x00;
	msg[7] = 0x92;
}

static void sendFrame101Ff() {
	CanTxMessage msg(CanCategory::NBC, 0x101, 8, COLT_CAN_BUS);
	msg[0] = 0xFF;
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame111() {
	CanTxMessage msg(CanCategory::NBC, 0x111, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x0B;
	msg[2] = 0x0C;
	msg[3] = 0xEE;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void processStartupInitSequence() {
	if (g_startupInitSequence5msTick == 0) {
		return;
	}

	if (g_startupInitSequenceRefresh) {
		switch (g_startupInitSequence5msTick) {
		case 1:
			sendFrame101Primary();
			break;

		case 4:
			sendFrame111();
			break;

		case 5:
			sendFrame101Ff();
			break;

		case 8:
			sendFrame111();
			g_startupInitSequence5msTick = 0;
			return;

		default:
			break;
		}
	} else {
		switch (g_startupInitSequence5msTick) {
		case 10:
			sendFrame101Primary();
			break;

		case 13:
			sendFrame111();
			break;

		case 14:
			sendFrame101Ff();
			break;

		case 15:
			sendFrame111();
			g_startupInitSequenceSent = true;
			g_startupInitSequence5msTick = 0;
			return;

		default:
			break;
		}
	}

	g_startupInitSequence5msTick++;
}

static void startStartupInitSequence(bool refresh) {
	if (g_startupInitSequence5msTick != 0) {
		return;
	}

	g_startupInitSequenceRefresh = refresh;
	g_startupInitSequence5msTick = 1;
}

static void sendFrame1E1() {
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

static void trigger1E1ClearBurst() {
	g_1e1ClearBurst5msTicks = COLT_1E1_CLEAR_BURST_5MS_TICKS;
	sendFrame1E1();
	sendFrame1E1();
	sendFrame1E1();
}

static void trigger1E1ClearOnly() {
	sendFrame1E1();
	sendFrame1E1();
	sendFrame1E1();
}

static void sendFrame210() {
	CanTxMessage msg(CanCategory::NBC, 0x210, 8, COLT_CAN_BUS);
	msg[0] = 0x00;
	msg[1] = 0x00;
	// OEM key-on behavior shows only a very short "01" phase here.
	msg[2] = (!isEngineRunning() && g_ignitionOn20msTicks <= 1) ? 0x01 : 0x00;
	msg[3] = 0x40;
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
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x68;

	if (!isEngineRunning()) {
		msg[1] = 0x37;
		msg[5] = 0xDA;
	} else {
		const int rpm = getCurrentRpm();
		if (rpm >= 1500) {
			msg[1] = 0x20;
			msg[5] = 0x2F;
		} else if (rpm >= 1200) {
			msg[1] = 0x28;
			msg[5] = 0x31;
		} else if (rpm >= 1000) {
			msg[1] = 0x37;
			msg[5] = 0x34;
		} else if (rpm >= 900) {
			msg[1] = 0x37;
			msg[5] = 0x37;
		} else {
			msg[1] = 0x37;
			msg[5] = 0x3A;
		}
	}

	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame308() {
	const uint16_t rawRpm = encodeColtDashRpm(Sensor::getOrZero(SensorType::Rpm));

	CanTxMessage msg(CanCategory::NBC, 0x308, 8, COLT_CAN_BUS);

	msg[1] = (rawRpm >> 8) & 0xFF;
	msg[2] = rawRpm & 0xFF;
	if (isEngineRunning()) {
		msg[0] = 0x80;
		msg[3] = 0x00;
		msg[4] = 0x00;
		msg[5] = 0x53;
		msg[6] = 0xFF;
	} else {
		msg[0] = 0x80;
		if (g_ignitionOn20msTicks <= COLT_MIL_BULB_CHECK_20MS_TICKS) {
			msg[3] = 0x06;
			msg[4] = 0x01;
		} else {
			msg[3] = 0x04;
			msg[4] = (g_ignitionOn20msTicks <= COLT_MIL_SELF_CHECK_SETTLE_20MS_TICKS) ? 0x01 : 0x00;
		}
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
	msg[1] = 0x71;
	msg[2] = 0x07;
	msg[3] = 0x71;
	msg[4] = 0x09;
	msg[5] = 0x02;
	msg[6] = 0x07;
	msg[7] = 0x91;
}

static void sendFrame416() {
	CanTxMessage msg(CanCategory::NBC, 0x416, 8, COLT_CAN_BUS);
	msg[0] = isEngineRunning() ? 0x8E : 0x75;
	msg[1] = 0x00;
	msg[2] = 0x00;
	msg[3] = 0x00;
	msg[4] = 0x00;
	msg[5] = 0x00;
	msg[6] = 0x00;
	msg[7] = 0x00;
}

static void sendFrame608() {
	CanTxMessage msg(CanCategory::NBC, 0x608, 8, COLT_CAN_BUS);

	if (isEngineRunning() || isColtFanTestHot()) {
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
	g_1e1ClearBurst5msTicks = 0;
	g_2f1ClearCooldown5msTicks = 0;
	g_startupInitRepeat20msTicks = 0;
	g_startupInitSequence5msTick = 0;
	g_startupInitSequenceSent = false;
	g_startupInitSequenceRefresh = false;
	g_wasIgnitionOn = false;
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
	const bool ignitionOn = isIgnitionOn();

	if (cycle.isInterval(CI::_5ms)) {
		processStartupInitSequence();
		if (g_1e1ClearBurst5msTicks > 0) {
			sendFrame1E1();
			sendFrame1E1();
			sendFrame1E1();
			sendFrame1E1();
			g_1e1ClearBurst5msTicks--;
		}

		if (g_2f1ClearCooldown5msTicks > 0) {
			g_2f1ClearCooldown5msTicks--;
		}
	}

	if (!ignitionOn) {
		if (g_wasIgnitionOn && g_startupInitSequence5msTick == 0) {
			g_startupInitSequenceSent = false;
		}

		g_wasIgnitionOn = false;
		g_ignitionOn20msTicks = 0;
		g_startupInitRepeat20msTicks = 0;
		return;
	}

	if (!g_wasIgnitionOn) {
		startStartupInitSequence(false);
	}

	g_wasIgnitionOn = true;

	if (cycle.isInterval(CI::_20ms)) {
		g_ignitionOn20msTicks++;
		g_startupInitRepeat20msTicks++;

		if (g_startupInitRepeat20msTicks >= COLT_STARTUP_INIT_REPEAT_20MS_TICKS) {
			startStartupInitSequence(true);
			g_startupInitRepeat20msTicks = 0;
		}

		sendFrame210();
		sendFrame212();
		sendFrame308();
		sendFrame312();
	}

	if (cycle.isInterval(CI::_100ms)) {
		sendFrame416();
		sendFrame608();
	}
#else
	UNUSED(cycle);
#endif
}

void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc) {
#if !defined(EFI_BOOTLOADER) && EFI_CAN_SUPPORT
	if (id == 0x002 && dlc >= 2 && data[0] == 0x00 && data[1] == 0x00) {
		if (!g_startupInitSequenceSent && g_startupInitSequence5msTick == 0) {
			startStartupInitSequence(false);
		}
	}

	if (id == 0x1E1 && dlc > 0 && data[0] == 0x81) {
		trigger1E1ClearBurst();
	}

	if (id == 0x2F1 && dlc >= 2 && data[0] == 0x00 && data[1] == 0x81) {
		if (g_2f1ClearCooldown5msTicks == 0) {
			trigger1E1ClearOnly();
			g_2f1ClearCooldown5msTicks = COLT_2F1_CLEAR_COOLDOWN_5MS_TICKS;
		}
	}

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
