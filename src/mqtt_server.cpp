#include "mqtt_server.h"
#include "connected_devices.h"
#include "modem.h"
#include "topics.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

extern bool publishMQTT(String topic, String message);
extern bool wifiConnected;
extern WiFiClientSecure espClient;
extern PubSubClient client;
extern bool mobileDataConnected;
extern int lastMotionStatus;
extern String lastMotionTime;
extern String lastSensorData;
extern Adafruit_MCP23X17 mcp;

HTTPClient http;

const char *broker_host = MQTT_SERVER;
const uint16_t broker_port = MQTT_PORT;
const char *broker_username = MQTT_USER;
const char *broker_password = MQTT_PASSWORD;
const char *client_id = "T-SIMCAM-LTE";
bool stream = false;

static CameraSettings makeDefaultSettings() {
	CameraSettings cfg;
	cfg.mode = "mode1";
	cfg.resolution = "5"; // VGA default
	cfg.quality = 12;
	cfg.brightness = 0;
	cfg.contrast = 0;
	cfg.motorX = 0;
	cfg.motorY = 0;
	cfg.hFlip = false;
	cfg.vFlip = false;
	cfg.hwDownscale = false;
	cfg.awb = true;
	cfg.aec = true;
	cfg.phoneNumber = "";
	cfg.emailAddress = "";
	cfg.sendSMS = false;
	cfg.sendEmail = false;
	cfg.monday = false;
	cfg.tuesday = false;
	cfg.wednesday = false;
	cfg.thursday = false;
	cfg.friday = false;
	cfg.saturday = false;
	cfg.sunday = false;
	cfg.startTime = "00:00";
	cfg.endTime = "23:59";
	cfg.sensorInterval = 5;
	return cfg;
}

CameraSettings currentSettings = makeDefaultSettings();

template <typename Setter> static void setIfPresent(const JsonVariantConst &root, const char *key, Setter setter, bool &updated) {
	JsonVariantConst v = root[key];
	if (!v.isNull()) {
		setter(v);
		updated = true;
	}
}

static framesize_t optionToFrameSize(const String &option) {
	if (option == "1") return FRAMESIZE_UXGA;
	if (option == "2") return FRAMESIZE_SXGA;
	if (option == "3") return FRAMESIZE_XGA;
	if (option == "4") return FRAMESIZE_SVGA;
	if (option == "5") return FRAMESIZE_VGA;
	if (option == "6") return FRAMESIZE_CIF;
	return FRAMESIZE_VGA;
}

static String frameSizeToOption(framesize_t size) {
	switch (size) {
	case FRAMESIZE_UXGA:
		return "1";
	case FRAMESIZE_SXGA:
		return "2";
	case FRAMESIZE_XGA:
		return "3";
	case FRAMESIZE_SVGA:
		return "4";
	case FRAMESIZE_VGA:
		return "5";
	case FRAMESIZE_CIF:
		return "6";
	default:
		return "5";
	}
}

static void refreshSettingsFromSensor(CameraSettings &settings) {
	if (!cameraReady) {
		return;
	}
	sensor_t *sensor = esp_camera_sensor_get();
	if (!sensor) {
		return;
	}

	settings.resolution = frameSizeToOption((framesize_t)sensor->status.framesize);
	settings.quality = sensor->status.quality;
	settings.brightness = sensor->status.brightness;
	settings.contrast = sensor->status.contrast;
	settings.hFlip = sensor->status.hmirror;
	settings.vFlip = sensor->status.vflip;
	settings.hwDownscale = sensor->status.dcw;
	settings.awb = sensor->status.awb;
	settings.aec = sensor->status.aec;
}

static bool applyCameraSettingsInternal(const CameraSettings &settings) {
	if (!cameraReady) {
		cameraReady = setupCamera();
	}
	sensor_t *sensor = esp_camera_sensor_get();
	if (!sensor) {
		Serial.println("Camera sensor not available for applying settings");
		return false;
	}

	framesize_t frame = optionToFrameSize(settings.resolution);
	bool ok = true;
	ok &= sensor->set_framesize(sensor, frame) == 0;
	ok &= sensor->set_quality(sensor, settings.quality) == 0;
	ok &= sensor->set_brightness(sensor, settings.brightness) == 0;
	ok &= sensor->set_contrast(sensor, settings.contrast) == 0;
	ok &= sensor->set_hmirror(sensor, settings.hFlip) == 0;
	ok &= sensor->set_vflip(sensor, settings.vFlip) == 0;
	ok &= sensor->set_dcw(sensor, settings.hwDownscale) == 0;
	ok &= sensor->set_whitebal(sensor, settings.awb) == 0;
	ok &= sensor->set_exposure_ctrl(sensor, settings.aec) == 0;

	if (!ok) {
		Serial.println("One or more camera settings failed to apply");
	}
	return ok;
}

static bool applyJsonToSettings(const JsonVariantConst &root, CameraSettings &settings) {
	bool updated = false;

	setIfPresent(root, "mode", [&](JsonVariantConst v) { settings.mode = v.as<String>(); }, updated);
	setIfPresent(root, "resolution", [&](JsonVariantConst v) { settings.resolution = v.as<String>(); }, updated);
	setIfPresent(root, "quality", [&](JsonVariantConst v) { settings.quality = v.as<int>(); }, updated);
	setIfPresent(root, "brightness", [&](JsonVariantConst v) { settings.brightness = v.as<int>(); }, updated);
	setIfPresent(root, "contrast", [&](JsonVariantConst v) { settings.contrast = v.as<int>(); }, updated);
	setIfPresent(root, "motorX", [&](JsonVariantConst v) { settings.motorX = v.as<int>(); }, updated);
	setIfPresent(root, "motorY", [&](JsonVariantConst v) { settings.motorY = v.as<int>(); }, updated);
	setIfPresent(root, "hFlip", [&](JsonVariantConst v) { settings.hFlip = v.as<bool>(); }, updated);
	setIfPresent(root, "horizontalFlip", [&](JsonVariantConst v) { settings.hFlip = v.as<bool>(); }, updated);
	setIfPresent(root, "vFlip", [&](JsonVariantConst v) { settings.vFlip = v.as<bool>(); }, updated);
	setIfPresent(root, "verticalFlip", [&](JsonVariantConst v) { settings.vFlip = v.as<bool>(); }, updated);
	setIfPresent(root, "hwDownscale", [&](JsonVariantConst v) { settings.hwDownscale = v.as<bool>(); }, updated);
	setIfPresent(root, "awb", [&](JsonVariantConst v) { settings.awb = v.as<bool>(); }, updated);
	setIfPresent(root, "aec", [&](JsonVariantConst v) { settings.aec = v.as<bool>(); }, updated);
	setIfPresent(root, "phoneNumber", [&](JsonVariantConst v) { settings.phoneNumber = v.as<String>(); }, updated);
	setIfPresent(root, "emailAddress", [&](JsonVariantConst v) { settings.emailAddress = v.as<String>(); }, updated);
	setIfPresent(root, "sendSMS", [&](JsonVariantConst v) { settings.sendSMS = v.as<bool>(); }, updated);
	setIfPresent(root, "sendEmail", [&](JsonVariantConst v) { settings.sendEmail = v.as<bool>(); }, updated);
	setIfPresent(root, "monday", [&](JsonVariantConst v) { settings.monday = v.as<bool>(); }, updated);
	setIfPresent(root, "tuesday", [&](JsonVariantConst v) { settings.tuesday = v.as<bool>(); }, updated);
	setIfPresent(root, "wednesday", [&](JsonVariantConst v) { settings.wednesday = v.as<bool>(); }, updated);
	setIfPresent(root, "thursday", [&](JsonVariantConst v) { settings.thursday = v.as<bool>(); }, updated);
	setIfPresent(root, "friday", [&](JsonVariantConst v) { settings.friday = v.as<bool>(); }, updated);
	setIfPresent(root, "saturday", [&](JsonVariantConst v) { settings.saturday = v.as<bool>(); }, updated);
	setIfPresent(root, "sunday", [&](JsonVariantConst v) { settings.sunday = v.as<bool>(); }, updated);
	setIfPresent(root, "startTime", [&](JsonVariantConst v) { settings.startTime = v.as<String>(); }, updated);
	setIfPresent(root, "endTime", [&](JsonVariantConst v) { settings.endTime = v.as<String>(); }, updated);
	setIfPresent(root, "sensorInterval", [&](JsonVariantConst v) { settings.sensorInterval = v.as<int>(); }, updated);

	return updated;
}

static String serializeSettings(const CameraSettings &settings) {
	JsonDocument doc;
	doc["mode"] = settings.mode;
	doc["resolution"] = settings.resolution;
	doc["quality"] = settings.quality;
	doc["brightness"] = settings.brightness;
	doc["contrast"] = settings.contrast;
	doc["motorX"] = settings.motorX;
	doc["motorY"] = settings.motorY;
	doc["hFlip"] = settings.hFlip;
	doc["horizontalFlip"] = settings.hFlip;
	doc["vFlip"] = settings.vFlip;
	doc["verticalFlip"] = settings.vFlip;
	doc["hwDownscale"] = settings.hwDownscale;
	doc["awb"] = settings.awb;
	doc["aec"] = settings.aec;
	doc["phoneNumber"] = settings.phoneNumber;
	doc["emailAddress"] = settings.emailAddress;
	doc["sendSMS"] = settings.sendSMS;
	doc["sendEmail"] = settings.sendEmail;
	doc["monday"] = settings.monday;
	doc["tuesday"] = settings.tuesday;
	doc["wednesday"] = settings.wednesday;
	doc["thursday"] = settings.thursday;
	doc["friday"] = settings.friday;
	doc["saturday"] = settings.saturday;
	doc["sunday"] = settings.sunday;
	doc["startTime"] = settings.startTime;
	doc["endTime"] = settings.endTime;
	doc["sensorInterval"] = settings.sensorInterval;

	String json;
	serializeJson(doc, json);
	return json;
}

static void persistCameraSettings(const CameraSettings &settings) {
	String json = serializeSettings(settings);
	saveCameraSettingsToPrefs(json);
	saveCameraSettingsToSD(json);
}

static bool loadSettingsFromStorage(CameraSettings &settings) {
	String json;
	if (!loadCameraSettingsFromPrefs(json)) {
		if (!loadCameraSettingsFromSD(json)) {
			return false;
		}
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, json);
	if (error) {
		Serial.println("Failed to parse stored settings JSON: " + String(error.c_str()));
		return false;
	}

	applyJsonToSettings(doc.as<JsonVariantConst>(), settings);
	return true;
}

void publishSettingsState() {
	String payload = serializeSettings(currentSettings);
	if (!publishMQTT(settingsTopic, payload)) {
		Serial.println("Failed to publish settings to MQTT");
	} else {
		Serial.println("Published current settings to MQTT");
	}
}

void initCameraSettings() {
	currentSettings = makeDefaultSettings();

	CameraSettings stored = currentSettings;
	refreshSettingsFromSensor(stored);
	if (loadSettingsFromStorage(stored)) {
		currentSettings = stored;
		bool applied = applyCameraSettingsInternal(currentSettings);
		if (applied) {
			refreshSettingsFromSensor(currentSettings);
		}
	} else {
		persistCameraSettings(currentSettings);
	}
}

void mqtt_callback_wrapper(char *topic, uint8_t *payload, unsigned int len) {
	mqtt_callback(topic, payload, len);
}

bool mqtt_connect_manualLTE() {
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
	if (modem.waitResponse(10000) != 1) {
		Serial.println("Failed to start MQTT service");
		return false;
	}
	delay(1000);

	modem.sendAT("+CMQTTACCQ=0,\"", client_id, "\",1"); // ziskanie MQTT klienta
	if (modem.waitResponse(5000) != 1) {
		Serial.println("Failed to acquire MQTT client");
		return false;
	}

	modem.sendAT("+CMQTTCFG=\"version\",0,4"); // nastav MQTT verziu 4 = 3.1.1
	if (modem.waitResponse() != 1) {
		Serial.println("Failed to set MQTT version");
		return false;
	}

	modem.sendAT("+CSSLCFG=\"sslversion\",0,4"); // nastav SSL verziu 4 = TLS 1.2
	if (modem.waitResponse() != 1) {
		Serial.println("Failed to set SSL version");
	}

	modem.sendAT("+CSSLCFG=\"enableSNI\",0,1"); // Povol Server Name Indication
	if (modem.waitResponse() != 1) {
		Serial.println("Failed to enable SNI");
	}

	modem.sendAT("+CSSLCFG=\"authmode\",0,0"); // 0 = bez autentigikacie servera
	if (modem.waitResponse() != 1) {
		Serial.println("Failed to set auth mode");
	}

	Serial.println("Enabling SSL for MQTT...");
	modem.sendAT("+CMQTTSSLCFG=0,0"); // SSL pre MQTT - client_id=0, ssl_ctx_index=0
	if (modem.waitResponse() != 1) {
		Serial.println("SSL config failed!");
		return false;
	}

	modem.sendAT("+CMQTTCONNECT=0,\"tcp://", broker_host, ":", String(broker_port), "\",60,1,\"", broker_username, "\",\"", broker_password, "\"");

	if (modem.waitResponse(30000) != 1) {
		Serial.println(" MQTT SSL connection failed!");

		modem.sendAT("+CMQTTCONNECT?"); // Debug info
		modem.waitResponse(2000);

		return false;
	}

	return true;
}

void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len) {
	const char *root = MQTT_TOPIC_ROOT;
	if (strncmp(topic, root, strlen(root)) != 0) {
		Serial.println("Ignoring MQTT topic outside root: " + String(topic));
		return;
	}

	if (len > 1600) {
		Serial.println("MQTT payload too large, ignoring");
		return;
	}

	Serial.println("MQTT Callback - Topic: " + String(topic) + ", Payload length: " + String(len));

	String payloadStr;
	payloadStr.reserve(len + 1);
	for (uint32_t i = 0; i < len; i++) {
		payloadStr += static_cast<char>(payload[i]);
	}

	if (strcmp(topic, commandTopic.c_str()) == 0) {
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, payloadStr);
		if (error) {
			Serial.println("Failed to parse command JSON: " + String(error.c_str()));
			return;
		}

		const char *type = doc["type"] | "";
		if (strcmp(type, "settings") == 0) {
			CameraSettings updated = currentSettings;
			bool changed = applyJsonToSettings(doc.as<JsonVariantConst>(), updated);
			if (!changed) {
				Serial.println("Settings command received but no fields changed");
				return;
			}

			bool applied = applyCameraSettingsInternal(updated);
			if (applied) {
				refreshSettingsFromSensor(updated);
			}
			currentSettings = updated;
			persistCameraSettings(currentSettings);
			publishSettingsState();
		} else if (strcmp(type, "motor") == 0) {
			Serial.println("Raw motor command JSON: " + payloadStr);

			// Frontend sends "pan" and "tilt", map them to x and y
			int x = doc["pan"] | currentSettings.motorX;
			int y = doc["tilt"] | currentSettings.motorY;

			Serial.printf("Motor command: pan=%d, tilt=%d\n", x, y);

			currentSettings.motorX = x;
			currentSettings.motorY = y;
			setMotorAngle(x, y);
			persistCameraSettings(currentSettings);
		} else if (strcmp(type, "get_settings") == 0) {
		refreshSettingsFromSensor(currentSettings);
		publishSettingsState();

		int pirState = mcp.digitalRead(MCP_PIR_PIN);
		publishMQTT(motionTopic, String(pirState));

		if (lastMotionTime.length() > 0) {
			publishMQTT(lastMotionTopic, lastMotionTime);
		}

		if (lastSensorData.length() > 0) {
			publishMQTT(temperatureTopic, lastSensorData);
		}
	} else {
		Serial.println("Unknown command type: " + String(type));
		}
	} else if (strcmp(topic, streamTopic.c_str()) == 0) {
		String ctrl = payloadStr;
		ctrl.trim();
		ctrl.toLowerCase();
		if (ctrl.length() != 0) {
			stream = (ctrl == "1" || ctrl == "on" || ctrl == "true");
		}
	} else if (strcmp(topic, snapshotTopic.c_str()) == 0) {

		if (!cameraReady) {
			if (!setupCamera()) {
				return;
			}
		}
		if (!sdReady) {
			sdInit();
			if (!sdReady) {
				return;
			}
		}

		camera_fb_t *fb = captureFrame();
		if (!fb) {
			return;
		}
		sdWritePhoto(("/photo" + String(photoNumber) + ".jpg").c_str(), fb);
		esp_camera_fb_return(fb);
		photoNumber++;
		savePhotoNumber(photoNumber);
	}
}

void mqttPrepareLTE() {
	modem.sendAT("+CMQTTSTOP"); // Zrus existujuci session
	modem.waitResponse(5000);

	bool enableSSL = true;
	bool enableSNI = true;
	modem.mqtt_begin(enableSSL, enableSNI);

	modem.mqtt_set_certificate(HivemqRootCA);
}

bool ensureWifiMqtt() {
	if (!wifiConnected) return false;
	if (!client.connected()) {
		espClient.stop();
		espClient.setInsecure();
		espClient.setHandshakeTimeout(30);
		client.setServer(MQTT_SERVER, MQTT_PORT);
		client.setKeepAlive(60);
		client.setBufferSize(2048);
		client.setSocketTimeout(20);
		if (!client.connect("ESP32Client", MQTT_USER, MQTT_PASSWORD)) {
			return false;
		}
		client.subscribe(commandTopic.c_str());
		client.subscribe(streamTopic.c_str());
		client.subscribe(snapshotTopic.c_str());
		client.subscribe(settingsTopic.c_str());
	}
	return true;
}

bool publishMQTT(String topic, String message) {
	bool published = false;

	// LTE path first
	if (mobileDataConnected) {
		modem.sendAT("+CMQTTDISC?"); // som pripojeny?

		if (modem.waitResponse(2000) != 1) {
			if (mqtt_connect_manualLTE()) {
				modem.mqtt_set_callback(mqtt_callback);
				modem.mqtt_subscribe(mqtt_client_id, temperatureTopic.c_str());
				modem.mqtt_subscribe(mqtt_client_id, commandTopic.c_str());
				modem.mqtt_subscribe(mqtt_client_id, streamTopic.c_str());
				modem.mqtt_subscribe(mqtt_client_id, snapshotTopic.c_str());
			} else {
				return false;
			}
		}

		if (!modem.mqtt_publish(mqtt_client_id, topic.c_str(), message.c_str())) { // publish
			return false;
		}
		return true;
	}

	// WiFi path
	if (wifiConnected) {
		if (!ensureWifiMqtt()) {
			return false;
		}

		published = client.publish(topic.c_str(), message.c_str());
	}

	return published;
}

bool postFrame() {
	if (!cameraReady || (!wifiConnected && !mobileDataConnected)) return false;

	camera_fb_t *fb = captureFrame();
	if (!fb) {
		return false;
	}

	Serial.printf("Frame: %d bytes, %dx%d\n", fb->len, fb->width, fb->height);

	String encoded = base64::encode(fb->buf, fb->len);
	Serial.printf("Base64: %d bytes\n", encoded.length());

	String payload = "{";
	payload += "\"name\":\"frame\",";
	payload += "\"data\":{";
	payload += "\"image\":\"" + encoded + "\",";
	payload += "\"timestamp\":" + String(millis()) + ",";
	payload += "\"width\":" + String(fb->width) + ",";
	payload += "\"height\":" + String(fb->height);
	payload += "}}";

	Serial.printf("Payload: %d bytes\n", payload.length());

	esp_camera_fb_return(fb);

	String url = "https://" + String(ABLY_HOST) + "/channels/" + ablyChannelName + "/messages";

	if (wifiConnected) {
		WiFiClientSecure ablyClient;
		ablyClient.setInsecure();

		http.begin(ablyClient, url);
		http.addHeader("Content-Type", "application/json");
		http.addHeader("Authorization", "Basic " + String(ABLY_AUTH_BASIC));
		http.setTimeout(8000);

		int httpResponseCode = http.POST(payload);
		if (httpResponseCode != 200 && httpResponseCode != 201) {
			Serial.print("Ably POST failed, HTTP response code: ");
			Serial.println(httpResponseCode);
			String response = http.getString();
			Serial.println("Response body: " + response);
			http.end();
			return false;
		}
		http.end();
	} else if (mobileDataConnected) {
		modem.sendAT("+HTTPTERM");
		modem.waitResponse(3000);

		modem.sendAT("+HTTPINIT");
		if (modem.waitResponse(10000) != 1) {
			Serial.println("HTTP init failed");
			return false;
		}

		modem.sendAT("+HTTPPARA=\"URL\",\"" + url + "\"");
		if (modem.waitResponse(3000) != 1) {
			Serial.println("HTTP set URL failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.sendAT("+HTTPPARA=\"CONTENT\",\"application/json\"");
		if (modem.waitResponse(3000) != 1) {
			Serial.println("HTTP set content type failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.sendAT("+HTTPPARA=\"USERDATA\",\"Authorization: Basic " + String(ABLY_AUTH_BASIC) + "\"");
		if (modem.waitResponse(3000) != 1) {
			Serial.println("HTTP set authorization failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.sendAT("+HTTPPARA=\"TIMEOUT\",\"8000\"");
		modem.waitResponse();

		modem.sendAT("+HTTPDATA=" + String(payload.length()) + ",8000");
		if (modem.waitResponse("DOWNLOAD") != 1) {
			Serial.println("HTTP data failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.stream.write(payload.c_str(), payload.length());
		if (modem.waitResponse() != 1) {
			Serial.println("HTTP data send failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.sendAT("+HTTPACTION=1");
		if (modem.waitResponse(30000, "+HTTPACTION:") != 1) {
			Serial.println("HTTP action failed");
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		String response = modem.stream.readStringUntil('\n');
		int c1 = response.indexOf(',');
		int c2 = response.indexOf(',', c1 + 1);
		int method = response.substring(0, c1).toInt();
		int status = response.substring(c1 + 1, c2).toInt();
		int length = response.substring(c2 + 1).toInt();

		if (status != 200 && status != 201) {
			Serial.print("Ably POST failed via LTE, HTTP response code: ");
			Serial.println(status);
			if (length > 0) {
				modem.sendAT("+HTTPREAD=0," + String(length));
				if (modem.waitResponse(3000) == 1) {
					String body = modem.stream.readStringUntil('\n');
					Serial.println("Response body: " + body);
				}
			}
			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
			return false;
		}

		modem.sendAT("+HTTPTERM");
		modem.waitResponse();
	}
	return true;
}

extern bool connectAbly() {
	if (!wifiConnected) return false;

	String url = "https://" + String(ABLY_HOST) + "/channels/" + ablyChannelName;

	WiFiClientSecure ablyClient;
	ablyClient.setInsecure();

	http.begin(ablyClient, url);
	http.addHeader("Authorization", "Basic " + String(ABLY_AUTH_BASIC));

	int httpResponseCode = http.GET();

	if (httpResponseCode != 200 && httpResponseCode != 201) {
		Serial.print("Ably connection failed, HTTP response code: ");
		Serial.println(httpResponseCode);
		http.end();
		return false;
	}

	http.end();
	return true;
}
