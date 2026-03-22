#include "pch.h"
#include "defaults.h"
#include "hellen_meta.h"
#include "board_overrides.h"
#include "connectors/generated_board_pin_names.h"
#include "colt_can.h"

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

static void setupColtSensorInputs() {
	engineConfiguration->tps1_1AdcChannel = MM100_IN_TPS_ANALOG;
	engineConfiguration->clt.adcChannel = MM100_IN_CLT_ANALOG;
	engineConfiguration->iat.adcChannel = MM100_IN_IAT_ANALOG;

	// PPS, later activeren als je hem nu al definitief wilt meenemen
	// setPPSInputs(MM100_IN_PPS_ANALOG, MM100_IN_AUX2_ANALOG);

	// hall1 = cam, hall2 = crank
	engineConfiguration->camInputs[0] = Gpio::MM100_IN_D1;        // PE12
	engineConfiguration->triggerInputPins[0] = Gpio::MM100_IN_D2; // PE13

	engineConfiguration->vehicleSpeedSensorInputPin = Gpio::MM100_IN_D3;
}

static void setupColtIo() {
	engineConfiguration->mainRelayPin = Gpio::MM100_IGN7;     // B9 / OUT_LS_HOT1
	engineConfiguration->fuelPumpPin = Gpio::MM100_OUT_PWM2;  // B16 / OUT_LS4

	// fan via CAN -> ETACS, dus niet lokaal schakelen
	// laat weg of unassign alleen als jouw tree dat accepteert
	// engineConfiguration->fanPin = Gpio::Unassigned;

	// D10 = A/C button
	//engineConfiguration->acSwitch = Gpio::MM100_IN_BUTTON2;
	//engineConfiguration->acSwitchMode = PI_PULLUP;

	// Clutch later invullen als je de definitieve pinnaam hier ook direct wilt zetten
	// engineConfiguration->clutchDownPin = ...;
	// engineConfiguration->clutchDownPinMode = PI_PULLUP;
}

static void setupColtCan() {
	engineConfiguration->canTxPin = Gpio::MM100_CAN_TX; // PB13
	engineConfiguration->canRxPin = Gpio::MM100_CAN_RX; // PB12

	setHellenCan2();

	engineConfiguration->enableVerboseCanTx = true;
	// optioneel:
	// engineConfiguration->canBaudRate = B500KBPS;
}

static void setupColtDbw() {
	setupTLE9201IncludingStepper(
		Gpio::MM100_OUT_PWM3,
		Gpio::MM100_OUT_PWM4,
		Gpio::MM100_SPI2_MISO
	);

	setupTLE9201IncludingStepper(
		Gpio::MM100_OUT_PWM5,
		Gpio::MM100_SPI2_MOSI,
		Gpio::MM100_USB1ID,
		1
	);

	engineConfiguration->etbFunctions[0] = DC_Throttle1;
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

	// bevestigd uit jouw trigger snippets
	engineConfiguration->trigger.type = trigger_type_e::TT_36_2_1;
	engineConfiguration->vvtMode[0] = VVT_MITSUBISHI_4G69;

	// optioneel, als jij engine sync expliciet op intake/cam wilt forceren:
	// engineConfiguration->engineSyncCam = SYNC_CAM_1;
}

static void colt_boardConfigOverrides() {
	setHellenMegaEnPin();
	setHellenVbatt();
	hellenMegaSdWithAccelerometer();

	engineConfiguration->vrThreshold[0].pin = Gpio::MM100_OUT_PWM6;

	setHellenCan();
	setDefaultHellenAtPullUps();
}

bool validateBoardConfig() {
	if (engineConfiguration->can2RxPin != Gpio::B12) {
		setHellenCan2();
	}
	return true;
}

static void colt_boardDefaultConfiguration() {
	setInjectorPins();
	setIgnitionPins();
	setupColtDbw();

	setHellenMMbaro();

	setupColtCan();
	setupColtIo();
	setupColtSensorInputs();
	setupColtEngine();

	// onboard EGT via SPI3
	engineConfiguration->is_enabled_spi_3 = true;
	engineConfiguration->spi3misoPin = Gpio::C11;
	engineConfiguration->spi3sckPin = Gpio::C10;
	engineConfiguration->max31855_cs[0] = Gpio::A15;
	engineConfiguration->max31855spiDevice = SPI_DEVICE_3;

#ifndef EFI_BOOTLOADER
	setCommonNTCSensor(&engineConfiguration->clt, HELLEN_DEFAULT_AT_PULLUP);
	setCommonNTCSensor(&engineConfiguration->iat, HELLEN_DEFAULT_AT_PULLUP);
#endif

	setTPS1Calibration(100, 650);
	hellenWbo();

	initColtCan();
}

static void colt_slowCallback() {
#ifndef EFI_BOOTLOADER
	extern AemXSeriesWideband aem1;
	if (aem1.hasSeenRx) {
		Gpio green = getRunningLedPin();
		auto greenPort = getBrainPinPort(green);
		auto greenPin = getBrainPinIndex(green);
		palClearPad(greenPort, greenPin);
	}
#endif
}

static void colt_fastCallback() {
	processColtCanTx();
}

void setup_custom_board_overrides() {
	custom_board_DefaultConfiguration = colt_boardDefaultConfiguration;
	custom_board_ConfigOverrides = colt_boardConfigOverrides;
	custom_board_periodicSlowCallback = colt_slowCallback;
	custom_board_periodicFastCallback = colt_fastCallback;
}

Gpio getWarningLedPin() {
	return Gpio::Unassigned;
}

Gpio getCommsLedPin() {
	return Gpio::Unassigned;
}

Gpio getRunningLedPin() {
	return Gpio::Unassigned;
}

void setHellenCan2() {
}

void setHellenMegaEnPin(bool) {
}

void setHellenVbatt() {
}

void hellenMegaSdWithAccelerometer() {
}

void setHellenCan() {
}

void hellenWbo() {
}