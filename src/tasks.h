#pragma once
#include <Arduino.h>

void networkTask(void *parameter);

void sensorTask(void *parameter);

void streamTask(void *parameter);

void initializeMQTTPublishTask();

struct NotificationMessage {
	char emailSubject[64];
	char emailBody[128];
	char smsText[128];
	bool sendEmail;
	bool sendSMS;
};

struct MQTTPublishMessage {
	char topic[64];
	char message[256];
};

struct MotorCommand {
	int angleX;
	int angleY;
};
