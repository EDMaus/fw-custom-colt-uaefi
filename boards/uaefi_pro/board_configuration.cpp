/**
 * @file custom Colt UAEFI board_configuration.cpp
 *
 * Based on official boards/hellen/uaefi/board_configuration.cpp
 * with Colt-specific additions merged in.
 */

#include "pch.h"
#include "defaults.h"
#include "hellen_meta.h"
#include "board_overrides.h"
#include "connectors/generated_board_pin_names.h"
#include "colt_can.h"
#include "hellen_logic.h"

#ifndef EFI_BOOTLOADER
#include "AemXSeriesLambda.h"
#endif // EFI_BOOTLOADER

static void setInjectorPins() {
	engineConfiguration->injectionPins[0] = Gpio::MM100_INJ1;
	engineConfiguration->injectionPins[1] = Gpio::MM100_INJ2;
	engineConfiguration->injectionPins[2] = Gpio::MM100_INJ3;
	engineConfiguration->injectionPins[3] = Gpio::MM100_INJ4;
	engineConfiguration->injectionPins[4] = Gpio::MM100_INJ5;
	engineConfiguration->injectionPins[5] = Gpio::MM100_INJ6;
}

static void setIgnitionPins() {
	engineConfiguration->ignitionPins[0] = Gpio::MM100_IGN1;
	engineConfiguration->ignitionPins[1] = Gpio::MM100_IGN2;
	engineConfiguration->ignitionPins[2] = Gpio::MM100_IGN3;
	engineConfiguration->ignitionPins[3] = Gpio::MM100_IGN4;
	engineConfiguration->ignitionPins[4] = Gpio::MM100_IGN5;
	engineConfiguration->ignitionPins[5] = Gpio::MM100_IGN6;
}

static void setupDefaultSensorInputs() {
	// ETB TPS
	engineConfiguration->tps1_1AdcChannel = MM100_IN_TPS_ANALOG;
	engineConfiguration->tps1_2AdcChannel = MM100_IN_AUX1_ANALOG;

	// MAP
	engineConfiguration->map.sensor.hwChannel = PIN_D9;

	// PPS
	setPPSInputs(MM100_IN_PPS_ANALOG, MM100_IN_AUX2_ANALOG);

	// Temps
	engineConfiguration->clt.adcChannel = MM100_IN_CLT_ANALOG;
	engineConfiguration->iat.adcChannel = MM100_IN_IAT_ANALOG;

	// Colt-specific trigger/cam mapping
	engineConfiguration->camInputs[0] = Gpio::MM100_IN_D1;        // PE12 = cam
	engineConfiguration->triggerInputPins[0] = Gpio::MM100_IN_D2; // PE13 = crank

	// VSS
	engineConfiguration->vehicleSpeedSensorInputPin = Gpio::MM100_IN_D3;
}

static void colt_boardConfigOverrides() {
        engineConfiguration->vrThreshold[0].pin = Gpio::MM100_OUT_PWM6;
        setDefaultHellenAtPullUps();
}

bool validateBoardConfig() {
        return true;
}

static void setUaefiDefaultETBPins() {
	// Official UAEFI ETB defaults
	setupTLE9201IncludingStepper(
		/*PWM controlPin*/ Gpio::MM100_OUT_PWM3,
		Gpio::MM100_OUT_PWM4,
		Gpio::MM100_SPI2_MISO
	);

	setupTLE9201IncludingStepper(
		/*PWM controlPin*/ Gpio::MM100_OUT_PWM5,
		Gpio::MM100_SPI2_MOSI,
		Gpio::MM100_USB1ID,
		1
	);
}

static void setupColtIo() {
	// Keep official UAEFI relay/fuel defaults, except where Colt differs
	engineConfiguration->mainRelayPin = Gpio::MM100_IGN7;      // B9
	engineConfiguration->fuelPumpPin = Gpio::MM100_OUT_PWM2;   // B16

	// Fan goes via CAN -> ETACS, so free IGN8 for starter control
	engineConfiguration->fanPin = Gpio::Unassigned;

	// OEM-style start request + starter relay
	engineConfiguration->startStopButtonPin = Gpio::MM100_IN_VSS; // D5 / IN_FLEX from your notes
	engineConfiguration->startStopButtonMode = PI_DEFAULT;
	engineConfiguration->startRequestPinInverted = false;

	engineConfiguration->starterControlPin = Gpio::MM100_IGN8; // B8

	// Optional A/C input later if needed
	// engineConfiguration->acSwitch = Gpio::MM100_IN_BUTTON2;
	// engineConfiguration->acSwitchMode = PI_PULLUP;
}

static void setupColtCan() {
	engineConfiguration->canTxPin = Gpio::MM100_CAN_TX; // PB13
	engineConfiguration->canRxPin = Gpio::MM100_CAN_RX; // PB12

#if (EFI_CAN_BUS_COUNT >= 3)
	engineConfiguration->can3TxPin = Gpio::MM100_CAN3_TX;
	engineConfiguration->can3RxPin = Gpio::MM100_CAN3_RX;
#endif

	engineConfiguration->enableVerboseCanTx = false;
	engineConfiguration->canBaudRate = B500KBPS;
}

static void setupColtEngine() {
	engineConfiguration->displacement = 1.468f;
	engineConfiguration->cylindersCount = 4;
	engineConfiguration->firingOrder = FO_1_3_4_2;

	engineConfiguration->injectionMode = IM_SEQUENTIAL;
	engineConfiguration->ignitionMode = IM_INDIVIDUAL_COILS;

	setAlgorithm(engine_load_mode_e::LM_SPEED_DENSITY);
	engineConfiguration->injectorCompensationMode = ICM_FixedRailPressure;

	engineConfiguration->displayLogicLevelsInEngineSniffer = true;
	engineConfiguration->isSdCardEnabled = true;
	engineConfiguration->enableSoftwareKnock = true;

	engineConfiguration->trigger.type = trigger_type_e::TT_36_2_1;
	engineConfiguration->vvtMode[0] = VVT_MITSUBISHI_4G69;
}

static void colt_boardDefaultConfiguration() {
	// Keep official UAEFI foundation
	setInjectorPins();
	setIgnitionPins();
	setUaefiDefaultETBPins();

	setHellenMMbaro();

	// Colt-specific board config
	setupDefaultSensorInputs();
	setupColtCan();
	setupColtIo();
	setupColtEngine();

	// On-board EGT via SPI3
	engineConfiguration->is_enabled_spi_3 = true;
	engineConfiguration->spi3misoPin = Gpio::C11;
	engineConfiguration->spi3sckPin = Gpio::C10;
	engineConfiguration->max31855_cs[0] = Gpio::A15;
	engineConfiguration->max31855spiDevice = SPI_DEVICE_3;

	engineConfiguration->etbFunctions[0] = DC_Throttle1;

#ifndef EFI_BOOTLOADER
	setCommonNTCSensor(&engineConfiguration->clt, HELLEN_DEFAULT_AT_PULLUP);
	setCommonNTCSensor(&engineConfiguration->iat, HELLEN_DEFAULT_AT_PULLUP);
#endif // EFI_BOOTLOADER

	setTPS1Calibration(100, 650);
    hellenWbo();
	// Colt CAN init
	initColtCan();
}

static Gpio OUTPUTS[] = {
	Gpio::MM100_INJ6, // B1 injector output 6
	Gpio::MM100_INJ5, // B2 injector output 5
	Gpio::MM100_INJ4, // B3 injector output 4
	Gpio::MM100_INJ3, // B4 injector output 3
	Gpio::MM100_INJ2, // B5 injector output 2
	Gpio::MM100_INJ1, // B6 injector output 1
	Gpio::MM100_INJ7, // B7 Low Side output 1
	Gpio::MM100_IGN8, // B8 starter relay output in Colt config
	Gpio::MM100_IGN7, // B9 main relay
	Gpio::MM100_OUT_PWM2, // B16 fuel pump
	Gpio::MM100_OUT_PWM1, // B17 Low Side output 3
	Gpio::MM100_INJ8, // B18 Low Side output 2
	// high sides
	Gpio::MM100_IGN6, // B10 Coil 6
	Gpio::MM100_IGN4, // B11 Coil 4
	Gpio::MM100_IGN3, // B12 Coil 3
	Gpio::MM100_IGN5, // B13 Coil 5
	Gpio::MM100_IGN2, // B14 Coil 2
	Gpio::MM100_IGN1, // B15 Coil 1
};

int getBoardMetaOutputsCount() {
	return efi::size(OUTPUTS);
}

int getBoardMetaLowSideOutputsCount() {
	return getBoardMetaOutputsCount() - 6;
}

Gpio* getBoardMetaOutputs() {
	return OUTPUTS;
}

int getBoardMetaDcOutputsCount() {
	if (engineConfiguration->engineType == engine_type_e::HONDA_OBD1 ||
		engineConfiguration->engineType == engine_type_e::MAZDA_MIATA_NA6 ||
		engineConfiguration->engineType == engine_type_e::MAZDA_MIATA_NA94 ||
		engineConfiguration->engineType == engine_type_e::MAZDA_MIATA_NA96 ||
		engineConfiguration->engineType == engine_type_e::MAZDA_MIATA_NB1 ||
		engineConfiguration->engineType == engine_type_e::MAZDA_MIATA_NB2) {
		return 0;
	}
	return 2;
}

//static void colt_slowCallback() {
//#ifndef EFI_BOOTLOADER
	//extern AemXSeriesWideband aem1;
	//if (aem1.hasSeenRx) {
	//	Gpio green = getRunningLedPin();
	//	auto greenPort = getBrainPinPort(green);
	//	auto greenPin = getBrainPinIndex(green);
	//	palClearPad(greenPort, greenPin); // Hellen has inverted LED control
	//}
//#endif // EFI_BOOTLOADER
//}

static void colt_fastCallback();

void setup_custom_board_overrides() {
	custom_board_DefaultConfiguration = colt_boardDefaultConfiguration;
	custom_board_ConfigOverrides = colt_boardConfigOverrides;
	custom_board_periodicFastCallback = colt_fastCallback;
}

int boardGetAnalogInputDiagnostic(adc_channel_e hwChannel, float voltage) {
	(void)voltage;

	switch (hwChannel) {
		case MM100_IN_TPS_ANALOG:
		case MM100_IN_PPS_ANALOG:
		case MM100_IN_IAT_ANALOG:
		case MM100_IN_CLT_ANALOG:
		case MM100_IN_O2S_ANALOG:
		case MM100_IN_O2S2_ANALOG:
		case MM100_IN_MAP1_ANALOG:
		case MM100_IN_AUX1_ANALOG:
		case MM100_IN_AUX2_ANALOG:
		case MM100_IN_AUX4_ANALOG:
			return (boardGetAnalogDiagnostic() == ObdCode::None) ? 0 : -1;
		default:
			return 0;
	}
}

#ifdef EFI_BOOTLOADER
static void colt_fastCallback() {
}
#else
static void colt_fastCallback() {
	processColtCanTx();
}
#endif

Gpio getWarningLedPin() {
	return Gpio::Unassigned;
}

Gpio getCommsLedPin() {
	return Gpio::Unassigned;
}

Gpio getRunningLedPin() {
	return Gpio::Unassigned;
}