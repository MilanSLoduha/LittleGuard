#include "modem.h"

#ifdef DUMP_AT_COMMANDS
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

const uint8_t mqtt_client_id = 0;
uint32_t check_connect_millis = 0;

void setupModem() {
	SerialAT.begin(115200, SERIAL_8N1, PCIE_RX_PIN, PCIE_TX_PIN);

	pinMode(PWR_ON_PIN, OUTPUT);
	digitalWrite(PWR_ON_PIN, HIGH);
	delay(300);

	pinMode(PCIE_PWR_PIN, OUTPUT);
	digitalWrite(PCIE_PWR_PIN, LOW);
	delay(100);
	digitalWrite(PCIE_PWR_PIN, HIGH);
	delay(MODEM_PWRON_PWMS);
	digitalWrite(PCIE_PWR_PIN, LOW);

	Serial.println("Starting modem...");
	delay(3000);

	int retry = 0;
	while (!modem.testAT(1000)) {
		Serial.print(".");
		if (retry++ > 30) {
			// Restart
			digitalWrite(PCIE_PWR_PIN, LOW);
			delay(100);
			digitalWrite(PCIE_PWR_PIN, HIGH);
			delay(MODEM_PWRON_PWMS);
			digitalWrite(PCIE_PWR_PIN, LOW);
			retry = 0;
		}
	}

	// Wait for modem to be fully ready
	Serial.println("\nWaiting for SMS subsystem...");
	if (!modem.waitResponse(100000UL, "SMS DONE")) {
		Serial.println("Warning: SMS subsystem not ready");
	}
}

void initSIM() {
	SimStatus sim = SIM_ERROR;
	int retry = 0;

	while (sim != SIM_READY && retry < 20) {
		sim = modem.getSimStatus();
		if (sim == SIM_READY) {
			break;
		} else if (sim == SIM_LOCKED) {
			Serial.println("SIM locked");
		}
		delay(1000);
	}
}

bool connectMobileData() {
	int16_t sq;
	RegStatus status = REG_NO_RESULT;
	int timeout = 0;

	while ((status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) && timeout < 30) {
		status = modem.getRegistrationStatus();

		if (status == REG_UNREGISTERED || status == REG_SEARCHING) {
			sq = modem.getSignalQuality();
			Serial.print(".");
			delay(1000);
			timeout++;
		} else if (status == REG_DENIED) {
			Serial.println("\nNetwork registration DENIED!");
			return false;
		} else if (status == REG_OK_ROAMING) {
			Serial.println("\nRegistered (roaming)");
			break;
		} else
			break;
	}

	if (timeout >= 30) {
		Serial.println("\nTimeout network registration!");
		return false;
	}

	if (!modem.gprsConnect("o2internet", "", "")) {
		Serial.println("Failed with o2internet (trying fallback APN)");

		if (!modem.gprsConnect("internet.o2active", "", "")) {
			Serial.println("Failed with internet.o2active");
			return false;
		}
	}

	Serial.println(modem.getLocalIP());
	return true;
}