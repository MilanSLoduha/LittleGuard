#pragma once
#include <Arduino.h>

void networkTask(void *parameter);

void sensorTask(void *parameter);

struct NotificationMessage {
	char emailSubject[64];
	char emailBody[128];
	char smsText[128];
	bool sendEmail;
	bool sendSMS;
};

struct MotorCommand {
	int angleX;
	int angleY;
};
