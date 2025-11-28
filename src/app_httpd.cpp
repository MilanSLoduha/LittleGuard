#include "Arduino.h"
#include "HivemqRootCA.h"
#include "camera_index.h"
#include "esp_http_server.h"
#include "modem.h"
#include "mqtt_server.h"
#include "sd_storage.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Forward declarations
void processPairingRequest();
bool publishMQTT(String topic, String message);

extern bool wifiConnected;
extern bool mobileDataConnected;
extern bool saveCameraSetup(String &macAddress);
extern TinyGsm modem;

#define API_URL "https://littleguard.vercel.app/api/pairing/submit"

String pendingPairingCode = "";
String pendingMacAddress = "";
String pendingSSID = "";
String pendingPassword = "";
bool pairingPending = false;

httpd_handle_t camera_httpd = NULL;

static esp_err_t index_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/html");
	httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	return httpd_resp_send(req, (const char *)index_html_gz, index_html_gz_len);
}

static String getJsonValue(const String &body, const char *key) {
	String k = String("\"") + key + "\"";
	int pos = body.indexOf(k);
	if (pos == -1) return String("");
	int colon = body.indexOf(':', pos);
	if (colon == -1) return String("");
	int i = colon + 1;
	while (i < body.length() && isspace(body[i]))
		i++;
	if (i >= body.length()) return String("");
	if (body[i] == '"') {
		int start = i + 1;
		int end = body.indexOf('"', start);
		if (end == -1) return String("");
		return body.substring(start, end);
	} else {
		int end = body.indexOf(',', i);
		if (end == -1) end = body.indexOf('}', i);
		return body.substring(i, end);
	}
}

static esp_err_t setup_handler(httpd_req_t *req) {
	if (req->method != HTTP_POST) {
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}
	size_t len = req->content_len;
	if (len == 0) {
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}
	char *buf = (char *)malloc(len + 1);
	if (!buf) {
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}
	int ret = httpd_req_recv(req, buf, len);
	if (ret <= 0) {
		free(buf);
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}
	buf[len] = 0;
	String body = String(buf);
	free(buf);

	String ssid = getJsonValue(body, "ssid");
	String password = getJsonValue(body, "password");
	String pairing = getJsonValue(body, "pairing");

	Serial.printf("Setup request: ssid='%s' pairing=%s\n", ssid.c_str(), pairing.c_str());

	if (ssid.length() > 0) {
		WiFi.disconnect(true);
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid.c_str(), password.c_str());
		unsigned long start = millis();
		unsigned long timeout = 15000; // 15s
		while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
			delay(300);
		}
		if (WiFi.status() == WL_CONNECTED) {
			wifiConnected = true;
			mobileDataConnected = false;
			saveWIFICredentials(ssid, password);
			Serial.printf("WiFi connected IP=%s\n", WiFi.localIP().toString().c_str());
		} else {
			wifiConnected = false;
			Serial.println("WiFi connection failed");
		}
	}

	bool success = false;
	String sent_via = "none";
	String message = "";

	// Získaj MAC adresu zariadenia
	uint8_t mac[6];
	esp_efuse_mac_get_default(mac);
	char macStr[18];
	sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (wifiConnected && pairing.length() > 0) {
		pendingPairingCode = pairing;
		pendingMacAddress = String(macStr);
		pendingSSID = ssid;
		pendingPassword = password;
		pairingPending = true;

		xTaskCreate(
		    [](void *param) {
			    processPairingRequest();
			    vTaskDelete(NULL);
		    },
		    "pairing_task", 8192, NULL, 1, NULL);//8kb

		success = true;
		sent_via = "wifi";
		message = "Pairing request started in background task";
		Serial.println("Pairing task created");
	} else if (mobileDataConnected && pairing.length() > 0) {
		pendingPairingCode = pairing;
		pendingMacAddress = String(macStr);
		pendingSSID = ssid;
		pendingPassword = password;
		pairingPending = true;

		// Vytvor task pre LTE párovanie
		xTaskCreate(
		    [](void *param) {
			    processPairingRequest();
			    vTaskDelete(NULL);
		    },
		    "pairing_task_lte", 8192, NULL, 1, NULL);

		success = true;
		sent_via = "lte";
		message = "Pairing request started via LTE";
		Serial.println("LTE pairing task created");
	} else if (!wifiConnected && !mobileDataConnected) {
		Serial.println("No network connection available, cannot pair");
		message = "No network connection available";
	} else {
		message = "Invalid pairing code";
	}

	String reply = "{";
	reply += "\"success\":" + String(success ? "true" : "false") + ",";
	reply += "\"sent_via\":\"" + sent_via + "\",";
	reply += "\"message\":\"" + message + "\"}";
	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	return httpd_resp_send(req, reply.c_str(), reply.length());
}

void startCameraServer() {
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};

	httpd_uri_t setup_uri = {.uri = "/setup", .method = HTTP_POST, .handler = setup_handler, .user_ctx = NULL};

	Serial.printf("Starting web server on port: '%d'\n", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(camera_httpd, &index_uri);
		httpd_register_uri_handler(camera_httpd, &setup_uri);
	}
}

void processPairingRequest() {
	if (!pairingPending || (!wifiConnected && !mobileDataConnected)) {
		return;
	}

	pairingPending = false;

	Serial.println("Processing pairing request...");
	String jsonPayload = "{\"code\":\"" + pendingPairingCode + "\",\"mac\":\"" + pendingMacAddress + "\"}";

	bool success = false;

	if (wifiConnected) {
		WiFiClientSecure *client = new WiFiClientSecure;
		if (client) {
			client->setCACert(HivemqRootCA);

			HTTPClient https;
			https.begin(*client, API_URL);
			https.addHeader("Content-Type", "application/json");
			https.setTimeout(10000);

			int httpCode = https.POST(jsonPayload);

			if (httpCode > 0) {
				Serial.printf("HTTP Response code: %d\n", httpCode);
				String response = https.getString();
				Serial.println("Response: " + response);

				if (httpCode == 200) {
					success = true;
					saveCameraSetup(pendingMacAddress);
					saveFirstTime();
				} else {
					Serial.println("Pairing failed");
				}
			} else {
				Serial.printf("HTTPS POST failed, error: %s\n", https.errorToString(httpCode).c_str());
			}
			https.end();
			delete client;
		}
	} else if (mobileDataConnected) {
		bool lteHttpSuccess = true;

		modem.sendAT("+HTTPTERM");
		modem.waitResponse(3000);

		modem.sendAT("+HTTPINIT");
		if (modem.waitResponse(10000) != 1) {
			Serial.println("LTE HTTP init failed");
			lteHttpSuccess = false;
		}

		if (lteHttpSuccess) {
			modem.sendAT("+HTTPPARA=\"URL\",\"" + String(API_URL) + "\"");
			if (modem.waitResponse(3000) != 1) {
				Serial.println("LTE HTTP set URL failed");
				modem.sendAT("+HTTPTERM");
				modem.waitResponse();
				lteHttpSuccess = false;
			}
		}

		if (lteHttpSuccess) {
			modem.sendAT("+HTTPPARA=\"CONTENT\",\"application/json\"");
			if (modem.waitResponse(3000) != 1) {
				Serial.println("LTE HTTP set content type failed");
				modem.sendAT("+HTTPTERM");
				modem.waitResponse();
				lteHttpSuccess = false;
			}
		}

		if (lteHttpSuccess) {
			modem.sendAT("+HTTPPARA=\"TIMEOUT\",\"15000\"");
			modem.waitResponse();

			modem.sendAT("+HTTPDATA=" + String(jsonPayload.length()) + ",10000");
			if (modem.waitResponse("DOWNLOAD") != 1) {
				Serial.println("LTE HTTP data failed");
				modem.sendAT("+HTTPTERM");
				modem.waitResponse();
				lteHttpSuccess = false;
			}
		}

		if (lteHttpSuccess) {
			modem.stream.write(jsonPayload.c_str(), jsonPayload.length());
			if (modem.waitResponse() != 1) {
				Serial.println("LTE HTTP data send failed");
				modem.sendAT("+HTTPTERM");
				modem.waitResponse();
				lteHttpSuccess = false;
			}
		}

		if (lteHttpSuccess) {
			modem.sendAT("+HTTPACTION=1");
			if (modem.waitResponse(30000, "+HTTPACTION:") != 1) {
				Serial.println("LTE HTTP action failed");
				modem.sendAT("+HTTPTERM");
				modem.waitResponse();
				lteHttpSuccess = false;
			}
		}

		if (lteHttpSuccess) {
			String response = modem.stream.readStringUntil('\n');
			int c1 = response.indexOf(',');
			int c2 = response.indexOf(',', c1 + 1);
			int method = response.substring(0, c1).toInt();
			int status = response.substring(c1 + 1, c2).toInt();
			int length = response.substring(c2 + 1).toInt();

			if (status == 200) {
				Serial.println("LTE pairing successful");
				success = true;
				saveCameraSetup(pendingMacAddress);
				saveFirstTime();
			} else {
				Serial.printf("LTE pairing failed, HTTP status: %d\n", status);
				if (length > 0) {
					modem.sendAT("+HTTPREAD=0," + String(length));
					if (modem.waitResponse(3000) == 1) {
						String body = modem.stream.readStringUntil('\n');
						Serial.println("Response body: " + body);
					}
				}
			}

			modem.sendAT("+HTTPTERM");
			modem.waitResponse();
		}

		if (!lteHttpSuccess || !success) {
			Serial.println("LTE HTTP failed, trying MQTT...");
		}
	}

	if (!success) {
		Serial.println("HTTP failed, trying MQTT...");
		if(publishMQTT(settings_topic, jsonPayload)) {
			saveCameraSetup(pendingMacAddress);
			saveFirstTime();
		}
	}

	pendingPairingCode = "";
	pendingMacAddress = "";
	pendingSSID = "";
	pendingPassword = "";
}
