#include "tasks.h"
#include "camera.h"
#include "connected_devices.h"
#include "email_notification.h"
#include "esp_task_wdt.h"
#include "modem.h"
#include "mqtt_server.h"
#include "sd_storage.h"
#include "topics.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

extern QueueHandle_t motorCommandQueue;
extern QueueHandle_t mqttPublishQueue;
extern WiFiClientSecure espClient;
extern PubSubClient client;
extern bool wifiConnected;
extern bool mobileDataConnected;
extern bool stream;
extern bool pendingGetSettingsResponse;
extern long long lastFrame;
extern bool mcpReady;
extern Adafruit_MCP23X17 mcp;
extern int lastMotionStatus;
extern String lastMotionTime;
extern unsigned long lastNotificationTime;
extern const unsigned long NOTIFICATION_THRESHOLD_MS;
extern String lastSensorData;
extern unsigned long lastSensorRead;
extern bool rtcReady;
extern bool bme680Ready;
extern Adafruit_BME680 bme;
extern RTC_DS3231 rtc;
extern CameraSettings currentSettings;
extern String motionTopic;
extern String lastMotionTopic;
extern String temperatureTopic;

#define SEALEVELPRESSURE_HPA (1013.25)

static bool recordingInProgress = false;

static void motionRecordingTask(void *param) {
	uint32_t durationMs = *((uint32_t *)param);
	delete (uint32_t *)param;

	String savedPath;
	if (!recordMotionClip(durationMs, savedPath)) {
		Serial.println("Motion recording failed");
	} else {
		Serial.println("Motion recording finished");
	}
	recordingInProgress = false;
	vTaskDelete(NULL);
}

// Notification sending task - runs on Core 1 to avoid blocking networkTask
static void notificationSendTask(void *param) {
	NotificationMessage *notif = (NotificationMessage *)param;
	
	Serial.println("Starting notification send task...");
	
	if (notif->sendEmail && strlen(notif->emailSubject) > 0) {
		Serial.println("Sending email notification...");
		if (sendEmailNotification(String(notif->emailSubject), String(notif->emailBody))) {
			Serial.println("Email sent successfully");
		} else {
			Serial.println("Email sending failed");
		}
	}
	
	vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between operations
	
	if (notif->sendSMS && strlen(notif->smsText) > 0) {
		Serial.println("Sending SMS notification...");
		modem.sendSMS(currentSettings.phoneNumber, String(notif->smsText));
		Serial.println("SMS sent");
	}
	
	delete notif;
	Serial.println("Notification task completed");
	vTaskDelete(NULL);
}

static void mqttPublishTask(void *param) {
	MQTTPublishMessage msg;
	
	while (true) {
		// Block until message arrives, no timeout
		if (xQueueReceive(mqttPublishQueue, &msg, portMAX_DELAY) == pdTRUE) {
			Serial.printf("MQTT Publish Task: Publishing to %s\n", msg.topic);
			bool success = publishMQTT(String(msg.topic), String(msg.message));
			if (!success) {
				Serial.printf("MQTT Publish Task: Failed to publish to %s\n", msg.topic);
			}
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

static bool queueMqttPublish(const String &topic, const String &message, TickType_t waitTicks) {
	if (mqttPublishQueue == NULL) {
		return false;
	}

	MQTTPublishMessage msg = {};
	strncpy(msg.topic, topic.c_str(), sizeof(msg.topic) - 1);
	msg.topic[sizeof(msg.topic) - 1] = '\0';
	strncpy(msg.message, message.c_str(), sizeof(msg.message) - 1);
	msg.message[sizeof(msg.message) - 1] = '\0';

	return xQueueSend(mqttPublishQueue, &msg, waitTicks) == pdTRUE;
}

void initializeMQTTPublishTask() {
	xTaskCreatePinnedToCore(
		mqttPublishTask,
		"MQTTPublish",
		8192,
		NULL,
		1,
		NULL,
		1 
	);
	Serial.println("MQTT Publish Task initialized on Core 1");
}

void networkTask(void *parameter) {
	unsigned long lastMqttCheck = 0;
	const unsigned long MQTT_CHECK_INTERVAL = 100;
	unsigned long lastMqttPublish = 0;
	const unsigned long MQTT_PUBLISH_INTERVAL = 5 * 60 * 1000;

	for (;;) {
		if (millis() - lastMqttCheck >= MQTT_CHECK_INTERVAL) {
			if (wifiConnected && client.connected()) {
				client.loop();
			}
			if (mobileDataConnected) {
				if (xSemaphoreTake(modemMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
					modem.mqtt_handle();
					xSemaphoreGive(modemMutex);
				}
			}
			lastMqttCheck = millis();
		}

		if (millis() - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
			if (!lastMotionTime.isEmpty()) {
				MQTTPublishMessage msg;
				strncpy(msg.topic, lastMotionTopic.c_str(), sizeof(msg.topic) - 1);
				strncpy(msg.message, lastMotionTime.c_str(), sizeof(msg.message) - 1);
				xQueueSend(mqttPublishQueue, &msg, 0);
			}
			if (!lastSensorData.isEmpty()) {
				MQTTPublishMessage msg;
				strncpy(msg.topic, temperatureTopic.c_str(), sizeof(msg.topic) - 1);
				strncpy(msg.message, lastSensorData.c_str(), sizeof(msg.message) - 1);
				xQueueSend(mqttPublishQueue, &msg, 0);
			}
			if (lastMotionStatus >= 0) {
				MQTTPublishMessage msg;
				strncpy(msg.topic, motionTopic.c_str(), sizeof(msg.topic) - 1);
				String statusStr = String(lastMotionStatus);
				strncpy(msg.message, statusStr.c_str(), sizeof(msg.message) - 1);
				xQueueSend(mqttPublishQueue, &msg, 0);
			}
			lastMqttPublish = millis();
		}

		if (pendingGetSettingsResponse && mobileDataConnected) {
		publishSettingsState();
			// Queue motion status publish
			MQTTPublishMessage msg1;
			strncpy(msg1.topic, motionTopic.c_str(), sizeof(msg1.topic) - 1);
			String motionStr = String(mcp.digitalRead(MCP_PIR_PIN));
			strncpy(msg1.message, motionStr.c_str(), sizeof(msg1.message) - 1);
			xQueueSend(mqttPublishQueue, &msg1, 0);
			
			if (!lastMotionTime.isEmpty()) {
				MQTTPublishMessage msg2;
				strncpy(msg2.topic, lastMotionTopic.c_str(), sizeof(msg2.topic) - 1);
				strncpy(msg2.message, lastMotionTime.c_str(), sizeof(msg2.message) - 1);
				xQueueSend(mqttPublishQueue, &msg2, 0);
			}
			if (!lastSensorData.isEmpty()) {
				MQTTPublishMessage msg3;
				strncpy(msg3.topic, temperatureTopic.c_str(), sizeof(msg3.topic) - 1);
				strncpy(msg3.message, lastSensorData.c_str(), sizeof(msg3.message) - 1);
				xQueueSend(mqttPublishQueue, &msg3, 0);
			}
			pendingGetSettingsResponse = false;
		}

		NotificationMessage notif;
		if (xQueueReceive(notificationQueue, &notif, 0) == pdTRUE) {
			NotificationMessage *notifCopy = new NotificationMessage(notif);
			
			BaseType_t created = xTaskCreatePinnedToCore(
				notificationSendTask,
				"NotifSend",
				12288,
				notifCopy,
				1,
				NULL,
				1
			);
			
			if (created != pdPASS) {
				delete notifCopy;
				Serial.println("Failed to create notification task");
			} else {
				Serial.println("Notification task spawned on Core 1");
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void streamTask(void *parameter) {
	const unsigned long STREAM_INTERVAL_MS = 100;

	for (;;) {

		if (stream && mobileDataConnected) {
			if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
				if (postFrame()) {
					lastFrame = millis();
				} else {
					Serial.println("Failed to send LTE snapshot");
				}
				xSemaphoreGive(cameraMutex);
			}
			stream = false;
		} else if (stream && (millis() - lastFrame) >= STREAM_INTERVAL_MS) {
			if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
				if (postFrame()) {
					lastFrame = millis();
				} else {
					Serial.println("Failed to send frame");
				}
				xSemaphoreGive(cameraMutex);
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void sensorTask(void *parameter) {
	unsigned long lastPirCheck = 0;
	const unsigned long PIR_CHECK_INTERVAL = 1000;

	for (;;) {

		if (mcpReady && (millis() - lastPirCheck >= PIR_CHECK_INTERVAL)) {
			if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
				int pirState = mcp.digitalRead(MCP_PIR_PIN);
				xSemaphoreGive(i2cMutex);

				if (lastMotionStatus != pirState) {
					Serial.printf("DEBUG: PIR state change: %d -> %d\n", lastMotionStatus, pirState);
					
					// Vzdy odosli MQTT pri zmene stavu
					String motionPayload = String(pirState);
					bool motionPublished = false;
					for (uint8_t attempt = 0; attempt < 3 && !motionPublished; ++attempt) {
						motionPublished = publishMQTT(motionTopic, motionPayload);
						if (!motionPublished) {
							vTaskDelay(pdMS_TO_TICKS(50));
						}
					}
					if (!motionPublished) {
						Serial.println("Failed to publish motion MQTT status change");
					}

					if (rtcReady && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
						DateTime now = rtc.now();
						Serial.printf("Motion state changed to %d at %02d:%02d:%02d\n", pirState, now.hour(), now.minute(), now.second());
						lastMotionTime = stringTime(now);
						xSemaphoreGive(i2cMutex);
					}

					bool windowOk = isNotificationAllowed();
					
					if (windowOk && pirState == HIGH) {
						// Na nahravanie fotky/videa reaguj iba v pripade pohybu HIGH
						if (currentSettings.mode == "mode1" && !recordingInProgress) {
							recordingInProgress = true;
							uint32_t *durationMs = new uint32_t(10000);
							BaseType_t created = xTaskCreatePinnedToCore(motionRecordingTask, "motionRecording", 8192, durationMs, 1, NULL, 1);
							if (created != pdPASS) {
								recordingInProgress = false;
								delete durationMs;
								Serial.println("Failed to start recording task");
							}
						} else if (currentSettings.mode == "mode2") {
							if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
								camera_fb_t *fb = captureFrame();
								if (fb != NULL) {
									if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
										sdWritePhoto(("/photo" + String(photoNumber) + ".jpg").c_str(), fb);
										photoNumber++;
										savePhotoNumber(photoNumber);
										xSemaphoreGive(sdMutex);
									}
									esp_camera_fb_return(fb);
								}
								xSemaphoreGive(cameraMutex);
							}
						}
					}

					uint32_t currentTime = millis();
					bool canSendNotification = (currentTime - lastNotificationTime) >= NOTIFICATION_THRESHOLD_MS;
					bool motionDetected = (pirState == HIGH);

					if (canSendNotification && windowOk && motionDetected) {
						Serial.println("Sending notification (time window OK, threshold passed)");
						NotificationMessage notif;
						memset(&notif, 0, sizeof(NotificationMessage));
						String motionMessage = String("Na vasej kamere LittleGuard ") + getTopicMac() + " bol zaznamenany pohyb";

						if (currentSettings.sendEmail && currentSettings.emailAddress.length() > 0) {
							strncpy(notif.emailSubject, "LittleGuard - Zmena stavu senzora", 63);
							strncpy(notif.emailBody, motionMessage.c_str(), 127);
							notif.sendEmail = true;
						}

						if (currentSettings.sendSMS && currentSettings.phoneNumber.length() > 0) {
							strncpy(notif.smsText, motionMessage.c_str(), 127);
							notif.sendSMS = true;
						}

						if (xQueueSend(notificationQueue, &notif, 0) == pdTRUE) {
							lastNotificationTime = currentTime;
							Serial.println("Notification queued successfully");
						} else {
							Serial.println("Failed to queue notification");
						}
					} else {
						if (!canSendNotification) {
							Serial.printf("Notification skipped: too soon (%.1f min since last)\n", 
								(float)(currentTime - lastNotificationTime) / 60000.0f);
						}
						if (!windowOk) {
							Serial.println("Notification skipped: outside time window");
						}
						if (!motionDetected) {
							Serial.println("Notification skipped: PIR is LOW");
						}
					}

					lastMotionStatus = pirState;
				}

				lastPirCheck = millis();
			}
		}

		unsigned long sensorIntervalMs = currentSettings.sensorInterval * 60000UL;
		if (bme680Ready && (millis() - lastSensorRead) >= sensorIntervalMs) {
			if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
				unsigned long endTime = bme.beginReading();
				if (endTime != 0) {
					vTaskDelay(pdMS_TO_TICKS(1000));

					float tlak = bme.pressure / 100.0;
					float vlhkost = bme.humidity;
					float teplota = bme.temperature;
					float plyn = bme.gas_resistance / 1000.0;
					float nadmorskaVyska = bme.readAltitude(SEALEVELPRESSURE_HPA);

					String sensorData = "{";
					sensorData += "\"temperature\":" + String(teplota) + ",";
					sensorData += "\"pressure\":" + String(tlak) + ",";
					sensorData += "\"humidity\":" + String(vlhkost) + ",";
					sensorData += "\"gas\":" + String(plyn) + ",";
					sensorData += "\"altitude\":" + String(nadmorskaVyska);
					sensorData += "}";

					lastSensorData = sensorData;
					lastSensorRead = millis();
				}
				xSemaphoreGive(i2cMutex);
			}
		}

		uint8_t snapshotCmd;
		if (xQueueReceive(snapshotRequestQueue, &snapshotCmd, 0) == pdTRUE) {
			
			if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
				camera_fb_t *fb = captureFrame();
				if (fb != NULL) {
					if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
						sdWritePhoto(("/photo" + String(photoNumber) + ".jpg").c_str(), fb);
						photoNumber++;
						savePhotoNumber(photoNumber);
						xSemaphoreGive(sdMutex);
					}
					esp_camera_fb_return(fb);
				}
				xSemaphoreGive(cameraMutex);
			}
		}

		MotorCommand motorCmd;
		if (xQueueReceive(motorCommandQueue, &motorCmd, 0) == pdTRUE) {
			setMotorAngle(motorCmd.angleX, motorCmd.angleY);
		}

		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
