/////////////////////////////////////////////// MAIN CODE ////////////////////////////////////////////////////
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024
// #define DUMP_AT_COMMANDS

#include <Arduino.h>
#include <ESP.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <StreamDebugger.h>
#include <TinyGsmClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <stepper.h>

#include "WiFi.h"
#include "camera.h"
#include "connected_devices.h"
#include "email_notification.h"
#include "esp_task_wdt.h"
#include "modem.h"
#include "mqtt_server.h"
#include "reset.h"
#include "sd_storage.h"
#include "secrets.h"
#include "select_pins.h"
#include "tasks.h"
#include "topics.h"
#include <HTTPClient.h>

#define SerialMon Serial

// StreamDebugger debugger(SerialAT, Serial);//
// TinyGsm modem(debugger);
// TinyGsmHttpsComm<TinyGsmA7670, ASR_A7670X> https(modem);

int lastMotionStatus = -1;
String lastMotionTime = "";
String lastSensorData = "";
unsigned long lastNotificationTime = 0;
extern const unsigned long NOTIFICATION_THRESHOLD_MS = 5 * 60 * 1000; // 5 minút

// BME680 musí byť za camera.h kvôli konfliktu sensor_t
// senzor v namespace

WiFiClientSecure espClient;
PubSubClient client(espClient);
bool wifiConnected = false;
String SSID;
String PASSWORD;
bool mobileDataConnected = false;

bool firstRun;

long long lastFrame = 0;

extern HTTPClient http;

bool mcpReady = false;

extern bool rtcReady;
extern bool bme680Ready;
#define SEALEVELPRESSURE_HPA (1013.25)
#define BME680_ADDR 0x77

unsigned long lastSensorRead = -INT_MAX;

void startCameraServer();
void processPairingRequest();

void disableWifiRadio() {
	WiFi.disconnect(true, true);
	WiFi.mode(WIFI_OFF);
}

void webServer() {
	String ssid;
	uint8_t mac[8];
	esp_efuse_mac_get_default(mac);
	ssid = WIFI_AP_SSID;
	ssid += mac[0] + mac[1] + mac[2];
	WiFi.mode(WIFI_MODE_APSTA); // AP
	WiFi.softAP(ssid.c_str(), WIFI_AP_PASSWORD);
	// WiFi.mode(WIFI_STA);
	// WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	//  while (WiFi.status() != WL_CONNECTED) {
	//  	delay(500);
	//  	Serial.print(".");
	//  }

	Serial.println("");
	Serial.print("Connected to WiFi. IP: ");
	Serial.println(WiFi.localIP());

	startCameraServer();
	Serial.print("Camera Ready! Use 'http://");
	Serial.print(WiFi.localIP());
	Serial.println("' to connect");
}

bool wifiSetup() {
	WiFi.begin(SSID, PASSWORD);

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

void setup() {
	Serial.begin(115200);
	delay(100);

	initSharedResources();

	bool shouldFactoryReset = detectDoubleReset() || detectPowerCycleDoubleReset();
	if (shouldFactoryReset) { // 3500delay
		factoryReset();
	}
	indicateDoubleResetWindow();

	sdInit();

	initTopics();

	firstRun = firstTime();
	if (!firstRun) {
		String ssidLoaded;
		String passLoaded;
		bool credentials = loadWIFICredentials(ssidLoaded, passLoaded);

		wifiConnected = false;
		if (credentials) {
			SSID = ssidLoaded;
			PASSWORD = passLoaded;
			Serial.printf("Loaded WiFi credentials. SSID=\"%s\"\n", SSID.c_str());
			wifiConnected = wifiSetup();
			if (wifiConnected) {
				Serial.println("Connection mode: WiFi");
			}
		} else {
			Serial.println("Without credentials");
		}
		if (!wifiConnected) {
			disableWifiRadio(); // on LTE only, stop WiFi stack spam
		}
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

			modem.mqtt_subscribe(mqtt_client_id, temperatureTopic.c_str());
			modem.mqtt_subscribe(mqtt_client_id, commandTopic.c_str());
			modem.mqtt_subscribe(mqtt_client_id, streamTopic.c_str());
			modem.mqtt_subscribe(mqtt_client_id, snapshotTopic.c_str());
			modem.mqtt_subscribe(mqtt_client_id, settingsTopic.c_str());

			if (mobileDataConnected) {
				Serial.println("Connection mode: LTE");
			}

		} else {
			espClient.setInsecure();
			client.setServer(MQTT_SERVER, MQTT_PORT);
			client.setKeepAlive(60);
			client.setCallback(mqtt_callback_wrapper);
			client.setBufferSize(2048);

			if (!client.connected()) {
				if (!client.connect("ESP32Client", MQTT_USER, MQTT_PASSWORD)) {
					Serial.print("MQTT WiFi connect failed, rc=");
					Serial.println(client.state());
				} else {
					client.subscribe(commandTopic.c_str());
					client.subscribe(streamTopic.c_str());
					client.subscribe(snapshotTopic.c_str());
					client.subscribe(settingsTopic.c_str());
				}
			}
		}

		// I2C
		Wire.begin(SDA_PIN, SCL_PIN);

		if (!mcpReady) {
			mcpReady = mcp.begin_I2C(0x20);
			if (!mcpReady) {
				Serial.println("MCP23017 have not been found on 0x20!");
			}
		}
		if (mcpReady) {
			setupSensors();
			// setRTCTime();
		} 
		connectAbly();

		cameraReady = setupCamera();
		if (!cameraReady) {
			Serial.println("Camera setup failed");
		}
		initCameraSettings();
		// bme680Ready = false; // sddddddddddddddddddddddddddddddd

		xTaskCreatePinnedToCore(networkTask,        
		                        "NetworkTask",      
		                        8192,               
		                        NULL,               
		                        2,                  
		                        &networkTaskHandle, 
		                        0                   
		);

		xTaskCreatePinnedToCore(sensorTask,        
		                        "SensorTask",      
		                        16384,
		                        NULL,              
		                        1,                 
		                        &sensorTaskHandle, 
		                        1                  
		);

		xTaskCreatePinnedToCore(streamTask,        
		                        "StreamTask",      
		                        8192,
		                        NULL,              
		                        1,                 
		                        &streamTaskHandle, 
		                        0                  
		);

		Serial.println("Tasks created successfully");
	} else {
		disableWifiRadio();
		setupModem();
		initSIM();

		mobileDataConnected = connectMobileData();
		mqttPrepareLTE();

		if (!mqtt_connect_manualLTE()) {
			Serial.println("MQTT connection failed!");
		} else {
			modem.mqtt_set_callback(mqtt_callback);
		}
		if (mobileDataConnected) {
			Serial.println("Connection mode: LTE");
		}

		webServer();
	}
}

void loop() {
	if (firstRun) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	} else {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	Serial.clearWriteError();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////