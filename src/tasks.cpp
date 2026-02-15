#include "tasks.h"
#include "camera.h"
#include "connected_devices.h"
#include "email_notification.h"
#include "esp_task_wdt.h"
#include "modem.h"
#include "mqtt_server.h"
#include "sd_storage.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

extern QueueHandle_t motorCommandQueue;
extern WiFiClientSecure espClient;
extern PubSubClient client;
extern bool wifiConnected;
extern bool mobileDataConnected;
extern bool stream;
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

void networkTask(void *parameter) {
	esp_task_wdt_add(NULL);

	unsigned long lastMqttCheck = 0;
	const unsigned long MQTT_CHECK_INTERVAL = 100;
	unsigned long lastMqttPublish = 0;
	const unsigned long MQTT_PUBLISH_INTERVAL = 5 * 60 * 1000;

	for (;;) {
		esp_task_wdt_reset();

		if (millis() - lastMqttCheck >= MQTT_CHECK_INTERVAL) {
			if (wifiConnected && client.connected()) {
				client.loop();
			}
			if (mobileDataConnected) {
				modem.mqtt_handle();
			}
			lastMqttCheck = millis();
		}

		// Periodically publish sensor data and motion info
		if (millis() - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
			if (!lastMotionTime.isEmpty()) {
				publishMQTT(lastMotionTopic, lastMotionTime);
			}
			if (!lastSensorData.isEmpty()) {
				publishMQTT(temperatureTopic, lastSensorData);
			}
			if (lastMotionStatus >= 0) {
				publishMQTT(motionTopic, String(lastMotionStatus));
			}
			lastMqttPublish = millis();
		}

		if (stream && (millis() - lastFrame) >= 100) {
			if (xSemaphoreTake(cameraMutex, pdMS_TO_TICKS(150)) == pdTRUE) {
				if (postFrame()) {
					lastFrame = millis();
				} else {
					Serial.println("Failed to send frame");
				}
				xSemaphoreGive(cameraMutex);
			}
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

void sensorTask(void *parameter) {
	esp_task_wdt_add(NULL);

	unsigned long lastPirCheck = 0;
	const unsigned long PIR_CHECK_INTERVAL = 1000;

	for (;;) {
		esp_task_wdt_reset();

		if (mcpReady && (millis() - lastPirCheck >= PIR_CHECK_INTERVAL)) {
			if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
				int pirState = mcp.digitalRead(MCP_PIR_PIN);
				xSemaphoreGive(i2cMutex);

				if (lastMotionStatus != pirState) {
					if (pirState == HIGH) {
						if (rtcReady && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
							DateTime now = rtc.now();
							Serial.printf("Motion HIGH at %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
							xSemaphoreGive(i2cMutex);
						} else {
							Serial.println("Motion HIGH (no RTC)");
						}
						bool windowOk = isNotificationAllowed();
						if (!windowOk) {
							lastMotionStatus = pirState;
							lastPirCheck = millis();
							continue; // outside allowed window, skip actions
						}

						if (currentSettings.mode == "mode1" && !recordingInProgress) {
							recordingInProgress = true;
							uint32_t *durationMs = new uint32_t(10000);
							BaseType_t created = xTaskCreatePinnedToCore(
							    motionRecordingTask,
							    "motionRecording",
							    8192,
							    durationMs,
							    1,
							    NULL,
							    1);
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
						} else {
							// mode3 (or other): do nothing on motion
						}

						uint32_t currentTime = millis();
						bool canSendNotification = (currentTime - lastNotificationTime) >= NOTIFICATION_THRESHOLD_MS;

						if (rtcReady && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
							DateTime now = rtc.now();
							lastMotionTime = stringTime(now);
							xSemaphoreGive(i2cMutex);
						}

						if (canSendNotification && windowOk) {
						Serial.println("Sending notification (time window OK, threshold passed)");
						NotificationMessage notif;
						memset(&notif, 0, sizeof(NotificationMessage));

						if (currentSettings.sendEmail && currentSettings.emailAddress.length() > 0) {
							strncpy(notif.emailSubject, "LittleGuard - Detekcia pohybu", 63);
							strncpy(notif.emailBody, "Pohyb bol detekovaný na vašom zariadení LittleGuard.", 127);
							notif.sendEmail = true;
						}

						if (currentSettings.sendSMS && currentSettings.phoneNumber.length() > 0) {
							strncpy(notif.smsText, "LittleGuard: Pohyb detekovaný!", 127);
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
					}
					} else {
						Serial.println("Žiadny pohyb.");
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
