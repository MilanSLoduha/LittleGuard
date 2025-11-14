/* ////////////////////////////////////// LTE network and MQTT test code //////////////////////////////////
#include <Arduino.h>

#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024
#define DUMP_AT_COMMANDS

#include "secrets.h"
#include "HivemqRootCA.h"
#include <TinyGsmClient.h>
#include "select_pins.h"

#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);

const char *broker_host = MQTT_SERVER;
const uint16_t broker_port = 8883;
const char *broker_username = MQTT_USER;
const char *broker_password = MQTT_PASSWORD;
const char *client_id = "T-SIMCAM-LTE";

const char *temperature_topic = TEMPERATURE_TOPIC;
const char *motion_topic = MOTION_TOPIC;

const uint8_t mqtt_client_id = 0;
uint32_t check_connect_millis = 0;

bool mqtt_connect_manual() {
    modem.sendAT("+CMQTTDISC=0,120"); // odpoj vsetky existujuce pripojenia
    modem.waitResponse(5000);
    delay(500);

    modem.sendAT("+CMQTTREL=0"); // Uvolneine vsetkych zdrojov klienta
    modem.waitResponse(5000);
    delay(500);

    modem.sendAT("+CMQTTSTOP"); // Stop MQTT sluzby
    modem.waitResponse(5000);
    delay(2000);

    modem.sendAT("+CMQTTSTART"); // Spustenie MQTT sluzby
    if(modem.waitResponse(10000) != 1) {
        Serial.println("Failed to start MQTT service");
        return false;
    }
    delay(1000);

    modem.sendAT("+CMQTTACCQ=0,\"", client_id, "\",1"); // ziskanie MQTT klienta
    if(modem.waitResponse(5000) != 1) {
        Serial.println("Failed to acquire MQTT client");
        return false;
    }

    modem.sendAT("+CMQTTCFG=\"version\",0,4"); // nastav MQTT verziu 4 = 3.1.1
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to set MQTT version");
        return false;
    }

    modem.sendAT("+CSSLCFG=\"sslversion\",0,4");  // nastav SSL verziu 4 =
TLS 1.2 if(modem.waitResponse() != 1) { Serial.println("Failed to set SSL
version");
    }

    modem.sendAT("+CSSLCFG=\"enableSNI\",0,1");  // Povol Server Name Indication
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to enable SNI");
    }

    modem.sendAT("+CSSLCFG=\"authmode\",0,0");  // 0 = bez autentigikacie
servera if(modem.waitResponse() != 1) { Serial.println("Failed to set auth
mode");
    }

    Serial.println("Enabling SSL for MQTT...");
    modem.sendAT("+CMQTTSSLCFG=0,0");  // SSL pre MQTT - client_id=0,
ssl_ctx_index=0 if(modem.waitResponse() != 1) { Serial.println("SSL config
failed!"); return false;
    }

    modem.sendAT("+CMQTTCONNECT=0,\"tcp://", broker_host, ":",
String(broker_port),
                 "\",60,1,\"", broker_username, "\",\"", broker_password, "\"");

    if(modem.waitResponse(30000) != 1) {
        Serial.println(" MQTT SSL connection failed!");

        modem.sendAT("+CMQTTCONNECT?"); // Debug info
        modem.waitResponse(2000);

        return false;
    }

    return true;
}

void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len) {
  Serial.println(topic);
  for (int i = 0; i < len; i++) {
      Serial.print((char)payload[i]);
  }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

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

    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY) {
        sim = modem.getSimStatus();
        if(sim == SIM_READY) {
            break;
        } else if(sim == SIM_LOCKED) {
            Serial.println("SIM locked");
        }
        delay(1000);
    }

    if(!modem.setNetworkAPN("o2internet")) {
        Serial.println("trying 'internet.o2active'");
        modem.setNetworkAPN("internet.o2active");
    }

    //cakanie na siet
    int16_t sq;
    RegStatus status = REG_NO_RESULT;
    while (status == REG_NO_RESULT || status == REG_SEARCHING || status ==
REG_UNREGISTERED) { status = modem.getRegistrationStatus(); if(status ==
REG_UNREGISTERED || status == REG_SEARCHING) { sq = modem.getSignalQuality();
        delay(1000);
      }
      else if(status == REG_DENIED) {
          Serial.println("\nNetwork registration denied!");
          return;
      }
      else if(status == REG_OK_ROAMING) {
          Serial.println("\nRegistered (roaming)");
          break;
      }
      else break;
    }

    Serial.print("Signal quality: ");
    Serial.println(modem.getSignalQuality());

    // Activate network
    if(!modem.setNetworkActive()) {
        Serial.println(" Failed to activate network");
    }


    modem.sendAT("+CMQTTSTOP"); // Zrus existujuci session
    modem.waitResponse(5000);

    bool enableSSL = true;
    bool enableSNI = true;
    modem.mqtt_begin(enableSSL, enableSNI);

    modem.mqtt_set_certificate(HivemqRootCA);

    delay(1000);

    // Connect using manual AT commands
    if(!mqtt_connect_manual()) {
        Serial.println("MQTT connection failed!");
        return;
    }

    modem.mqtt_set_callback(mqtt_callback);

    modem.mqtt_subscribe(mqtt_client_id, temperature_topic);
}

void loop() {
    if (millis() > check_connect_millis) {
        check_connect_millis = millis() + 10000UL;

        modem.sendAT("+CMQTTDISC?"); // som pripojeny?
        if(modem.waitResponse(2000) != 1) {
            Serial.println("MQTT disconnected, reconnecting...");
            mqtt_connect_manual();
        }
        else {
            String message = "T-SIMCAM LTE uptime: " + String(millis() / 1000) +
"s";

            if(!modem.mqtt_publish(mqtt_client_id, motion_topic,
message.c_str(), 1)) { Serial.println("Publish failed");
            }
        }
    }

    modem.mqtt_handle();
    delay(100);
}
*/ //////////////////////////////////////////////////////////////////////////////////////////////

/* /////////////////////////////////////////////// send sms //////////////////////////////////////////////

#define TINY_GSM_MODEM_A7670
#define LILYGO_T_SIMCAM

#include <Arduino.h>
#include "select_pins.h"  // All pin definitions and modem config

#define SerialMon Serial
#define SMS_TARGET "+421908199904"

// Include TinyGSM (modem type already defined at top of file)
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif


// It depends on the operator whether to set up an APN. If some operators do not
set up an APN,
// they will be rejected when registering for the network. You need to ask the
local operator for the specific APN.
// APNs from other operators are welcome to submit PRs for filling.
// #define NETWORK_APN     "CHN-CT"             //CHN-CT: China Telecom

void setup()
{
    Serial.begin(115200);
#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    // Set modem reset pin ,reset modem
#ifdef MODEM_RESET_PIN
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif

#ifdef MODEM_FLIGHT_PIN
    // If there is an airplane mode control, you need to exit airplane mode
    pinMode(MODEM_FLIGHT_PIN, OUTPUT);
    digitalWrite(MODEM_FLIGHT_PIN, HIGH);
#endif

    // Pull down DTR to ensure the modem is not in sleep state
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);

    // Turn on modem
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

#ifdef MODEM_RING_PIN
    // Set ring pin input
    pinMode(MODEM_RING_PIN, INPUT_PULLUP);
#endif

    // Set modem baud
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    Serial.println("Start modem...");
    delay(3000);

    while (!modem.testAT()) {
        delay(10);
    }


    // Wait PB DONE
    Serial.println("Wait SMS Done.");
    if (!modem.waitResponse(100000UL, "SMS DONE")) {
        Serial.println("Can't wait from sms register ....");
        return;
    }


#ifdef NETWORK_APN
    Serial.printf("Set network apn : %s\n", NETWORK_APN);
    if (!modem.setNetworkAPN(NETWORK_APN)) {
        Serial.println("Set network apn error !");
    }
#endif


    // Check network registration status and network signal status
    int16_t sq ;
    Serial.print("Wait for the modem to register with the network.");
    RegStatus status = REG_NO_RESULT;
    while (status == REG_NO_RESULT || status == REG_SEARCHING || status ==
REG_UNREGISTERED) { status = modem.getRegistrationStatus(); switch (status) {
        case REG_UNREGISTERED:
        case REG_SEARCHING:
            sq = modem.getSignalQuality();
            Serial.printf("[%lu] Signal Quality:%d\n", millis() / 1000, sq);
            delay(1000);
            break;
        case REG_DENIED:
            Serial.println("Network registration was rejected, please check if
the APN is correct"); return ; case REG_OK_HOME: Serial.println("Online
registration successful"); break; case REG_OK_ROAMING: Serial.println("Network
registration successful, currently in roaming mode"); break; default:
            Serial.printf("Registration Status:%d\n", status);
            delay(1000);
            break;
        }
    }
    Serial.println();


    Serial.printf("Registration Status:%d\n", status);
    delay(1000);

    Serial.print("Init success, start to send message to  ");
    Serial.println(SMS_TARGET);

    String imei = modem.getIMEI();
    bool res = modem.sendSMS(SMS_TARGET, String("Hello from ") + imei);
    Serial.print("Send sms message ");
    Serial.println(res ? "OK" : "fail");
}

void loop()
{

    if (SerialAT.available()) {
        Serial.write(SerialAT.read());
    }
    if (Serial.available()) {
        SerialAT.write(Serial.read());
    }
    delay(1);
}
*/

/* /////////////////////////////////////////////// MQTT TEST CODE ////////////////////////////////////////
#include "WiFi.h"
#include "camera.h"
#include "esp_task_wdt.h"
#include "rtc_time.h"
#include "sd_storage.h"
#include "secrets.h"
#include "select_pins.h"
#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <stepper.h>

// BME680 musí byť za camera.h kvôli konfliktu sensor_t
// senzor v namespace
#define sensor_t adafruit_sensor_t
#include "Adafruit_BME680.h"
#undef sensor_t

WiFiClientSecure espClient;
PubSubClient client(espClient);

Adafruit_MCP23X17 mcp;
RTC_DS3231 rtc;
Adafruit_BME680 bme;
bool bme680Ready = false;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME680_ADDR 0x77 // BME680

int lastState = -1;

// Topic premenné sú definované v mqtt_server.cpp
extern const char *temperature_topic;
extern const char *motion_topic;
extern const char *last_motion_topic;
extern const char *command_topic;
extern const char *settings_topic;
extern const char *stream_topic;
extern const char *snapshot_topic;

void setup() {
    Serial.begin(115200);
    delay(2000);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long wifiTimeout = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimeout) < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection FAILED - continuing without WiFi");
    }

    Wire.begin(SDA_PIN, SCL_PIN);

    espClient.setInsecure();
    client.setServer(MQTT_SERVER, MQTT_PORT);

    // Inicializácia MCP23017
    Serial.println("Looking for MCP23017...");
    bool mcpReady = false;
    if (mcp.begin_I2C(0x20)) {
        mcp.pinMode(MCP_SDA_PIN, OUTPUT);    // GPA0 ako SDA pre DS3231
        mcp.pinMode(MCP_SCL_PIN, OUTPUT);    // GPA1 ako SCL pre DS3231
        mcp.digitalWrite(MCP_SDA_PIN, HIGH); // Simulácia pull-up
        mcp.digitalWrite(MCP_SCL_PIN, HIGH);

        mcp.pinMode(MCP_PIR_PIN, INPUT);

        mcp.pinMode(OUTPUT1, OUTPUT);
        mcp.pinMode(OUTPUT2, OUTPUT);
        mcp.pinMode(OUTPUT3, OUTPUT);
        mcp.pinMode(OUTPUT4, OUTPUT);
        mcpReady = true;
    } else {
        Serial.println("WARNING: MCP23017 not found!");
    }

    if (mcpReady) {
        DateTime testTime;
    } else {
        Serial.println("Skipping RTC test - MCP not ready");
    }

    bme680Ready = false;

    if (bme.begin(0x77, &Wire)) {
        bme680Ready = true;
    }

    if (bme680Ready) {
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); // 320°C for 150 ms
        delay(2000);

        if (!bme.performReading()) {
            Serial.println("WARNING: BME680 detected but failed to perform reading");
            bme680Ready = false;
        }
    } else {
        Serial.println("WARNING: BME680 not found at address 0x77");
    }

    Serial.println("\n=== Setup complete! ===");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, skipping loop");
        delay(5000);
        return;
    }

    if (!client.connected()) {
        if (!client.connect("ESP32Client", MQTT_USER, MQTT_PASSWORD)) {
            Serial.print("MQTT connection failed, rc=");
            Serial.println(client.state());
            delay(2000);
            return;
        }
    }

    if (!bme680Ready) {
        Serial.println("BME680 not ready, skipping sensor reading");
        delay(10000);
        return;
    }

    unsigned long endTime = bme.beginReading();
    if (endTime == 0) {
        Serial.println(F("Failed to begin reading"));
        return;
    }

    // Serial.print(millis());
    // Serial.println(endTime);

    float tlak = bme.pressure / 100.0;                             // hPa
    float vlhkost = bme.humidity;                                  // %
    float teplota = bme.temperature;                               // °C
    float plyn = bme.gas_resistance / 1000.0;                      // KOhms
    float nadmorskaVyska = bme.readAltitude(SEALEVELPRESSURE_HPA); // m

    // odosielanie
    String sprava = String(bme.temperature);
    if (client.publish(temperature_topic, sprava.c_str())) {
        Serial.println("Message published successfully");
    } else {
        Serial.println("Message publishing failed");
    }

    int pirState = mcp.digitalRead(MCP_PIR_PIN);
    if (pirState != lastState) {
        if (lastState == 1) {
            DateTime now;

            sprava = stringTime(now);
            if (!client.publish(last_motion_topic, sprava.c_str())) {
                Serial.println("Last motion publish FAILED");
            }
        }
        lastState = pirState;
        sprava = String(pirState);
        if (!client.publish(motion_topic, sprava.c_str())) {
            Serial.println("Message publishing failed");
        }
    }
    client.loop();

    Serial.println();
    delay(10000);
}
*/ ///////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////// MAIN CODE ////////////////////////////////////////////////////
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024
#define DUMP_AT_COMMANDS

#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <StreamDebugger.h>
#include <TinyGsmClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <stepper.h>

#include "WiFi.h"
#include "camera.h"
#include "esp_task_wdt.h"
#include "modem.h"
#include "mqtt_server.h"
#include "rtc_time.h"
#include "sd_storage.h"
#include "secrets.h"
#include "select_pins.h"

#define SerialMon Serial
#define SMS_TARGET "+421908199904"

// StreamDebugger debugger(SerialAT, Serial);
// TinyGsm modem(debugger);

int lastMotionStatus = -1;
int currentMotorAngle = 0; // Aktuálna pozícia motora

// BME680 musí byť za camera.h kvôli konfliktu sensor_t
// senzor v namespace
#define sensor_t adafruit_sensor_t
#include "Adafruit_BME680.h"
#undef sensor_t

WiFiClientSecure espClient;
PubSubClient client(espClient);
bool wifiConnected = false;
bool mobileDataConnected = false;

Adafruit_MCP23X17 mcp;
bool mcpReady = false;
RTC_DS3231 rtc;
bool rtcReady = false;
Adafruit_BME680 bme;
bool bme680Ready = false;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME680_ADDR 0x77

void startCameraServer();

void setMotorAngle(int angle) {
	angle += 180;
	int steps = (abs(currentMotorAngle - angle) * 512) / 360;
	// 512 == 360 degrees

	if (currentMotorAngle > angle) {
		for (int j = 0; j < steps; j++) {
			mcp.digitalWrite(OUTPUT1, HIGH);
			mcp.digitalWrite(OUTPUT2, LOW);
			mcp.digitalWrite(OUTPUT3, LOW);
			mcp.digitalWrite(OUTPUT4, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, LOW);
			mcp.digitalWrite(OUTPUT2, LOW);
			mcp.digitalWrite(OUTPUT3, HIGH);
			mcp.digitalWrite(OUTPUT4, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, LOW);
			mcp.digitalWrite(OUTPUT2, HIGH);
			mcp.digitalWrite(OUTPUT3, HIGH);
			mcp.digitalWrite(OUTPUT4, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, HIGH);
			mcp.digitalWrite(OUTPUT2, HIGH);
			mcp.digitalWrite(OUTPUT3, LOW);
			mcp.digitalWrite(OUTPUT4, LOW);
			delay(DELAY);
		}
	} else {
		for (int i = 0; i < steps; i++) {
			mcp.digitalWrite(OUTPUT1, HIGH);
			mcp.digitalWrite(OUTPUT2, HIGH);
			mcp.digitalWrite(OUTPUT3, LOW);
			mcp.digitalWrite(OUTPUT4, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, LOW);
			mcp.digitalWrite(OUTPUT2, HIGH);
			mcp.digitalWrite(OUTPUT3, HIGH);
			mcp.digitalWrite(OUTPUT4, LOW);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, LOW);
			mcp.digitalWrite(OUTPUT2, LOW);
			mcp.digitalWrite(OUTPUT3, HIGH);
			mcp.digitalWrite(OUTPUT4, HIGH);
			delay(DELAY);
			mcp.digitalWrite(OUTPUT1, HIGH);
			mcp.digitalWrite(OUTPUT2, LOW);
			mcp.digitalWrite(OUTPUT3, LOW);
			mcp.digitalWrite(OUTPUT4, HIGH);
			delay(DELAY);
		}
	}
	currentMotorAngle = angle;
}

void webServer() {
	String ssid;
	uint8_t mac[8];
	esp_efuse_mac_get_default(mac);
	ssid = WIFI_AP_SSID;
	ssid += mac[0] + mac[1] + mac[2];
	// WiFi.mode(WIFI_MODE_APSTA); //AP
	// WiFi.softAP(ssid.c_str(), WIFI_AP_PASSWORD);
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.print("Connected to WiFi. IP: ");
	Serial.println(WiFi.localIP());

	startCameraServer();
	Serial.print("Camera Ready! Use 'http://");
	Serial.print(WiFi.localIP());
	Serial.println("' to connect");
}

void setupSensors() {
	bme680Ready = false;
	if (bme.begin(0x77, &Wire)) {
		bme680Ready = true;
	}

	if (bme680Ready) {
		bme.setTemperatureOversampling(BME680_OS_8X);
		bme.setHumidityOversampling(BME680_OS_2X);
		bme.setPressureOversampling(BME680_OS_4X);
		bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
		bme.setGasHeater(320, 150); // 320°C for 150 ms

		if (!bme.performReading()) {
			Serial.println("WARNING: BME680 detected but failed to perform reading");
			bme680Ready = false;
		}
	} else {
		Serial.println("WARNING: BME680 not found at address 0x77");
	}

	if (!rtc.begin(&Wire)) {
		Serial.println("WARNING: RTC DS3231 not found at 0x68");
	}

	mcp.pinMode(MCP_PIR_PIN, INPUT);

	// motor 1
	mcp.pinMode(OUTPUT1, OUTPUT);
	mcp.pinMode(OUTPUT2, OUTPUT);
	mcp.pinMode(OUTPUT3, OUTPUT);
	mcp.pinMode(OUTPUT4, OUTPUT);
}

bool wifiSetup() {
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	unsigned long wifiTimeout = millis();
	while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimeout) < 15000) {
		delay(500);
		Serial.print(".");
	}
	Serial.println();

	if (WiFi.status() == WL_CONNECTED) {
		Serial.println(WiFi.localIP());
		return true;
	} else {
		Serial.println("WiFi connection FAILED - continuing without WiFi");
		return false;
	}
}

void publishMQTT(String topic, String message) {
	if (mobileDataConnected) {

		modem.sendAT("+CMQTTDISC?"); // som pripojeny?

		if (modem.waitResponse(2000) != 1) {
			Serial.println("MQTT disconnected, reconnecting...");
			mqtt_connect_manualLTE();
		}

		if (!modem.mqtt_publish(mqtt_client_id, topic.c_str(), message.c_str())) { // publish
			Serial.println("Publish failed");
		}

	} else if (wifiConnected) {
		if (!client.connected()) {
			espClient.setInsecure();
			if (!client.connect("ESP32Client", MQTT_USER, MQTT_PASSWORD)) {
				Serial.print("MQTT connection failed, rc=");
				Serial.println(client.state());
				delay(2000);
				return;
			}
			Serial.println("MQTT reconnected, subscribing...");
			client.subscribe(command_topic);
			client.subscribe(stream_topic);
			client.subscribe(snapshot_topic);
		}

		if (!client.publish(topic.c_str(), message.c_str())) {
			Serial.println("Message publishing failed");
		}
	}
}

void setup() {
	Serial.begin(115200);
	delay(5000);

	wifiConnected = wifiSetup();
	setupModem();
	initSIM();

	if (!wifiConnected) {
		mobileDataConnected = connectMobileData();
		mqttPrepareLTE();

		if (!mqtt_connect_manualLTE()) {
			Serial.println("MQTT connection failed!");
			return;
		}
		modem.mqtt_set_callback(mqtt_callback);

		modem.mqtt_subscribe(mqtt_client_id, temperature_topic);
		modem.mqtt_subscribe(mqtt_client_id, command_topic);
		modem.mqtt_subscribe(mqtt_client_id, stream_topic);
		modem.mqtt_subscribe(mqtt_client_id, snapshot_topic);
	} else {
		Serial.println("Setting up WiFi MQTT...");
		espClient.setInsecure();
		client.setServer(MQTT_SERVER, MQTT_PORT);
		client.setKeepAlive(60); // Keepalive 60 sekúnd namiesto default 15s
		client.setCallback(mqtt_callback);
		// Pripojenie a subscribe sa urobia v loop()
		Serial.println("WiFi MQTT setup complete (will connect in loop)");
	}

	// Inicializácia I2C pre MCP23017
	Wire.begin(SDA_PIN, SCL_PIN);

	if (!mcp.begin_I2C(0x20)) {
		Serial.println("MCP23017 have not been found on 0x20!");
	}
	mcpReady = true;

	if (mcpReady) {
		setupSensors();
	}

	webServer();

	// bool res = modem.sendSMS(SMS_TARGET, String("SDADASDASD"));
	// Serial.println(res ? "" : "fail");

	cameraReady = setupCamera();
	if (!cameraReady) {
		Serial.println("Camera setup failed");
	}
}

void loop() {
	// Volaj MQTT loop hneď na začiatku pre udržanie spojenia
	if (wifiConnected) client.loop();
	if (mobileDataConnected) modem.mqtt_handle();

	DateTime now;
	// printTime(now);

	int pirState = mcp.digitalRead(MCP_PIR_PIN);

	delay(1000);
	if (lastMotionStatus != pirState) {
		if (pirState == HIGH) {
			Serial.println("Pohyb detekovaný!");
		} else {
			Serial.println("Žiadny pohyb.");
		}
		lastMotionStatus = pirState;
		publishMQTT(motion_topic, String(pirState));
	}

	if (bme680Ready) {
		unsigned long endTime = bme.beginReading();
		if (endTime == 0) {
			Serial.println(F("Failed to begin reading"));
			return;
		}
		delay(1000);                                                   // počkaj na dokončenie čítania
		float tlak = bme.pressure / 100.0;                             // hPa
		float vlhkost = bme.humidity;                                  // %
		float teplota = bme.temperature;                               // °C
		float plyn = bme.gas_resistance / 1000.0;                      // KOhms
		float nadmorskaVyska = bme.readAltitude(SEALEVELPRESSURE_HPA); // m

		publishMQTT(temperature_topic, String(teplota));
	}

	Serial.clearWriteError();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ////////////////////////////////////////////// Test ESP ////////////////////////////////////////////////
#include "FS.h" #include
"HTTPClient.h" #include "SD.h" #include "WiFi.h" #include "select_pins.h"
#include "esp_camera.h"
#include <Arduino.h>
#include <WiFiAP.h>
#include <driver/i2s.h>


HardwareSerial SerialAT(1);
HTTPClient http_client;

void mic_init(void);
void check_sound(void);
void sd_test(void);
void wifi_scan_connect(void);
void pcie_test(void);
void camera_test(void);
void startCameraServer();


void setup()
{
    pinMode(PWR_ON_PIN, OUTPUT);
    digitalWrite(PWR_ON_PIN, HIGH);
    delay(100);
    Serial.begin(115200);
    delay(500);  // Počkaj, aby sa Serial stabilizoval
    Serial.println("\n\n=== T-SIMCAM self test START ===");

#ifdef CAM_IR_PIN
    //Teset IR Filter
    pinMode(CAM_IR_PIN, OUTPUT);
    Serial.println("Test IR Filter");
    int i = 3;
    while (i--) {
        digitalWrite(CAM_IR_PIN, 1 - digitalRead(CAM_IR_PIN)); delay(1000);
    }
#endif
    sd_test();
    pcie_test();
    //mic_init();
    //wifi_scan_connect();
    delay(2000);
    //camera_test();
    //check_sound();
}

void loop()
{
    //check_sound();
    Serial.print(".....");
    delay(1000);
}

void sd_test(void)
{

    SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SPI)) {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");

    if (cardType == CARD_MMC)
        Serial.println("MMC");
    else if (cardType == CARD_SD)
        Serial.println("SDSC");
    else if (cardType == CARD_SDHC)
        Serial.println("SDHC");
    else
        Serial.println("UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    SD.end();
    return;
}

void mic_init(void)
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = 160,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,
    };

    i2s_pin_config_t pin_config = {-1};
    pin_config.bck_io_num = MIC_IIS_SCK_PIN;
    pin_config.ws_io_num = MIC_IIS_WS_PIN;
    pin_config.data_in_num = MIC_IIS_DATA_PIN;
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

#define BUFFER_SIZE (4 * 1024)
uint8_t buffer[BUFFER_SIZE] = {0};
const int define_max = 16000;
const int define_avg = 8000;
const int define_zero = 50;
String timelong_str = "";
float val_avg = 0;
int16_t val_max = 0;
float val_avg_1 = 0;
int16_t val_max_1 = 0;

float all_val_avg = 0;
int32_t all_val_zero1 = 0;
int32_t all_val_zero2 = 0;
int32_t all_val_zero3 = 0;

int16_t val16 = 0;
uint8_t val1, val2;
uint32_t j = 0;
bool aloud = false;

void check_sound(void)
{
    size_t bytes_read;
    j = j + 1;
    i2s_read(I2S_NUM_0, (char *)buffer, BUFFER_SIZE, &bytes_read,
portMAX_DELAY);

    for (int i = 0; i < BUFFER_SIZE / 2; i++) {
        val1 = buffer[i * 2];
        val2 = buffer[i * 2 + 1];
        val16 = val1 + val2 * 256;
        if (val16 > 0) {
            val_avg = val_avg + val16;
            val_max = max(val_max, val16);
        }
        if (val16 < 0) {
            val_avg_1 = val_avg_1 + val16;
            val_max_1 = min(val_max_1, val16);
        }

        all_val_avg = all_val_avg + val16;

        if (abs(val16) >= 20)
            all_val_zero1 = all_val_zero1 + 1;
        if (abs(val16) >= 15)
            all_val_zero2 = all_val_zero2 + 1;
        if (abs(val16) > 5)
            all_val_zero3 = all_val_zero3 + 1;
    }

    if (j % 2 == 0 && j > 0) {
        val_avg = val_avg / BUFFER_SIZE;
        val_avg_1 = val_avg_1 / BUFFER_SIZE;
        all_val_avg = all_val_avg / BUFFER_SIZE;

        if (val_max > define_max && val_avg > define_avg && all_val_zero2 >
define_zero) aloud = true; else aloud = false;

        timelong_str = " high_max:" + String(val_max) + " high_avg:" +
String(val_avg) + " all_val_zero2:" + String(all_val_zero2);

        if (aloud) {
            timelong_str = timelong_str + " ##### ##### ##### ##### #####
#####"; Serial.println(timelong_str);
        }

        val_avg = 0;
        val_max = 0;

        val_avg_1 = 0;
        val_max_1 = 0;

        all_val_avg = 0;
        all_val_zero1 = 0;
        all_val_zero2 = 0;
        all_val_zero3 = 0;
    }

}

void wifi_scan_connect(void)
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.println("scan start");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " :
"*"); delay(10);
        }
    }
    Serial.println("");
    WiFi.disconnect();

    uint32_t last_m = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Timeout 10 sekúnd na pripojenie
    uint32_t timeout = 10000;
    while (WiFi.status() != WL_CONNECTED && (millis() - last_m) < timeout) {
        Serial.print(".");
        vTaskDelay(100);
    }
    Serial.println("");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection failed - timeout!");
        Serial.println("Continuing without WiFi...");
        return;
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("\r\n-- wifi connect success! --\r\n");
    Serial.printf("It takes %d milliseconds\r\n", millis() - last_m);
    delay(100);
    String rsp;
    bool is_get_http = false;

    // do {
    //     http_client.begin("https://www.baidu.com/");
    //     int http_code = http_client.GET();
    //     Serial.println(http_code);
    //     if (http_code > 0) {
    //         Serial.printf("HTTP get code: %d\n", http_code);
    //         if (http_code == HTTP_CODE_OK) {
    //             rsp = http_client.getString();
    //             Serial.println(rsp);
    //             is_get_http = true;
    //         } else {
    //             Serial.printf("fail to get http client,code:%d\n",
http_code);
    //         }
    //     } else {
    //         Serial.println("HTTP GET failed. Try again");
    //     }
    //     delay(3000);
    // } while (!is_get_http);

    // WiFi.disconnect();
    http_client.end();
}

void pcie_test(void)
{
    SerialAT.begin(115200, SERIAL_8N1, PCIE_RX_PIN, PCIE_TX_PIN);
    delay(100);
    pinMode(PCIE_PWR_PIN, OUTPUT);
    digitalWrite(PCIE_PWR_PIN, 1);
    delay(500);
    digitalWrite(PCIE_PWR_PIN, 0);
    delay(3000);
    Serial.println("Waking up PCI module");
    do {
        SerialAT.println("AT");
        delay(50);
    } while (!SerialAT.find("OK"));
    Serial.println("The PCI module has been awakened");

    Serial.println("Example Query the SIM card status");
    int opacko = 0;
    do {
        SerialAT.println("AT+CPIN?");
        delay(50);
        Serial.println(opacko);
    } while (!SerialAT.find("READY") && opacko++ < 100);
    Serial.println("SIM card has been identified");
}

void camera_test()
{
    Serial.println("Camera init");
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_Y2_PIN;
    config.pin_d1 = CAM_Y3_PIN;
    config.pin_d2 = CAM_Y4_PIN;
    config.pin_d3 = CAM_Y5_PIN;
    config.pin_d4 = CAM_Y6_PIN;
    config.pin_d5 = CAM_Y7_PIN;
    config.pin_d6 = CAM_Y8_PIN;
    config.pin_d7 = CAM_Y9_PIN;
    config.pin_xclk = CAM_XCLK_PIN;
    config.pin_pclk = CAM_PCLK_PIN;
    config.pin_vsync = CAM_VSYNC_PIN;
    config.pin_href = CAM_HREF_PIN;
    config.pin_sccb_sda = CAM_SIOD_PIN;
    config.pin_sccb_scl = CAM_SIOC_PIN;
    config.pin_pwdn = CAM_PWDN_PIN;
    config.pin_reset = CAM_RESET_PIN;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition

    // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
    //                      for larger pre-allocated frame buffer.
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    // camera init
    Serial.printf("Camera init");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);       // flip it back
        s->set_brightness(s, 1);  // up the brightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }
    // drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif

    String ssid;
    uint8_t mac[8];
    esp_efuse_mac_get_default(mac);
    ssid = WIFI_AP_SSID;
    ssid += mac[0] + mac[1] + mac[2];
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(ssid.c_str(), WIFI_AP_PASSWORD);

    startCameraServer();
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.softAPIP());
    Serial.println("' to connect");
    // while (!WiFi.softAPgetStationNum()) {
    //     delay(10);
    // }
    // delay(5000);
}
*/ /////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ///////////////////////////////////////// I2C Scanner cez MCP23017 /////////////////////////////////////

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(43, 44); // SDA=GPIO43, SCL=GPIO44
  Serial.println("I2C Scanner");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Skenujem...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C zariadenie na adrese 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  if (nDevices == 0) Serial.println("Žiadne I2C zariadenia!");
  delay(5000);
}
*/ ///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ////////////////////////////////////////////// Test Camera /////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("T-SIMCAM minimal startup");

  if (!setupCamera()) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }

  if (!sdInit()) {
    Serial.println("SD init failed - continuing without SD");
  }

  camera_fb_t *fb = captureFrame();
  if (!fb) {
    Serial.println("Camera capture failed (fb null)");
    return;
  }

  sdWritePhoto("/photo.jpg", fb);
  esp_camera_fb_return(fb);
  Serial.println("Capture done.");
}

void loop() {
  Serial.print(".");
  delay(5000);
}
*/ ////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ////////////////////////////////////// BME680 with Adafruit Library ////////////////////////////////////

Adafruit_MCP23X17 mcp;  // MCP23017 GPIO expander
Adafruit_BME680 bme;    // BME680 on hardware I2C (same bus as MCP23017)
bool bme680Ready = false;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME680_ADDR 0x77  // I2C adresa BME680

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== T-SIMCAM BME680 + MCP23017 on Hardware I2C ===");
  Serial.println("Initializing I2C on GPIO43 (SDA) and GPIO44 (SCL)");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!mcp.begin_I2C(0x20)) {
    Serial.println(" MCP23017 not found at 0x20! Check wiring.");
    while (1) delay(1000);
  }

  delay(100);

  if(bme.begin(BME680_ADDR)) {
    Serial.println("BME680 detected at 0x77!");

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms

    delay(2000);

    bme680Ready = true;
  } else {
    Serial.println(" BME680 initialization failed");
  }

  Serial.println("\nSetup complete!");
}


void loop() {
  if(!bme680Ready) {
    Serial.println("BME680 not ready, waiting...");
    delay(5000);
    return;
  }

  unsigned long endTime = bme.beginReading();
  if (endTime == 0) {
    Serial.println(F("Failed to begin reading :("));
    return;
  }
  Serial.print(F("Reading started at "));
  Serial.print(millis());
  Serial.print(F(" and will finish at "));
  Serial.println(endTime);

  Serial.println(F("You can do other work during BME680 measurement."));
  delay(50); // This represents parallel work.
  // There's no need to delay() until millis() >= endTime: bme.endReading()
  // takes care of that. It's okay for parallel work to take longer than
  // BME680's measurement time.

  // Obtain measurement results from BME680. Note that this operation isn't
  // instantaneous even if milli() >= endTime due to I2C/SPI latency.
  if (!bme.endReading()) {
    Serial.println(F("Failed to complete reading :("));
    return;
  }
  Serial.print(F("Reading completed at "));
  Serial.println(millis());

  Serial.print(F("Temperature = "));
  Serial.print(bme.temperature);
  Serial.println(F(" *C"));

  Serial.print(F("Pressure = "));
  Serial.print(bme.pressure / 100.0);
  Serial.println(F(" hPa"));

  Serial.print(F("Humidity = "));
  Serial.print(bme.humidity);
  Serial.println(F(" %"));

  Serial.print(F("Gas = "));
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(F(" KOhms"));

  Serial.print(F("Approx. Altitude = "));
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(F(" m"));

  Serial.println();
    delay(5000);


}

*/ ///////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ///////////////////////////////////////////// SMS TEST CODE ////////////////////////////////////////////
#define SerialAT Serial1

String phoneNumber = "+421908199904";

void sendSMS(String number, String message);
void pcie_test();

bool modemReady = false;

void pcie_test()
{
    SerialAT.begin(115200, SERIAL_8N1, PCIE_RX_PIN, PCIE_TX_PIN);
    delay(100);
    pinMode(PCIE_PWR_PIN, OUTPUT);
    digitalWrite(PCIE_PWR_PIN, 1);
    delay(500);
    digitalWrite(PCIE_PWR_PIN, 0);
    delay(3000);

    // Prebudenie modemu
    do {
        SerialAT.println("AT");
        delay(50);
    } while (!SerialAT.find("OK"));

    // Kontrola SIM karty
    int opacko = 0;
    do {
        SerialAT.println("AT+CPIN?");
        delay(50);
        Serial.println(opacko);
    } while (!SerialAT.find("READY") && opacko++ < 100);

    // Registrácia na sieť
    SerialAT.println("AT+CREG?");
    delay(2000);
    while(SerialAT.available()) Serial.write(SerialAT.read());

    // Kontrola signálu
    SerialAT.println("AT+CSQ");
    delay(1000);
    while(SerialAT.available()) Serial.write(SerialAT.read());

    // Text mode pre SMS
    SerialAT.println("AT+CMGF=1");
    delay(1000);
    String resp = SerialAT.readString();
    Serial.print("Response: "); Serial.println(resp);

    SerialAT.println("AT+CSCS=\"GSM\"");
    delay(1000);
    resp = SerialAT.readString();

    Serial.println("Modem fully initialized!");
}

void setup() {
  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);

  Serial.begin(115200);
  delay(2000);

  pcie_test();

  delay(1000);

  sendSMS(phoneNumber, "Ono to funguje :D");

}

void loop() {

  delay(100);
}

void sendSMS(String number, String message) {

  while(SerialAT.available()) SerialAT.read();

  SerialAT.print("AT+CMGS=\"");
  SerialAT.print(number);
  SerialAT.println("\"");

  uint32_t timeout = millis();
  bool promptReceived = false;

  while(millis() - timeout < 5000) {
    if(SerialAT.available()) {
      char c = SerialAT.read();
      Serial.write(c);
      if(c == '>') {
        promptReceived = true;
        break;
      }
    }
    delay(10);
  }

  if(!promptReceived) {
    Serial.println("\n ERROR: No '>' prompt received!");
    return;
  }

  SerialAT.print(message);
  delay(100);

  SerialAT.write(26); // Ctrl+Z na odoslanie

  timeout = millis();
  String response = "";
  bool success = false;

  while(millis() - timeout < 20000) {
    if(SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      Serial.write(c);

      if(response.indexOf("+CMGS:") != -1 ||
         (response.indexOf("OK") != -1 && response.length() > 10)) {
        success = true;
        break;
      }

      if(response.indexOf("ERROR") != -1 ||
         response.indexOf("FAIL") != -1) {
        break;
      }
    }
    delay(10);
  }

  Serial.println();
  if(success) {
    Serial.println("Sprava odoslana");
  } else {
    Serial.println("Odoslanie zlyhalo");
    Serial.print("Chyba: ");
    Serial.println(response);
  }
}
*/ /////////////////////////////////////////////////////////////////////////////////////////////////////////