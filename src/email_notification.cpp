#include "email_notification.h"
#include "mqtt_server.h"
#include <ESP_Mail_Client.h>

extern bool wifiConnected;
extern bool mobileDataConnected;
extern CameraSettings currentSettings;

bool sendEmailNotification(String subject, String body) {
	if (currentSettings.emailAddress.length() == 0) {
		return false;
	}

	// Email works over either WiFi or mobile data
	if (!wifiConnected && !mobileDataConnected) {
		return false;
	}

	SMTPSession smtp;

	Session_Config config;
	config.server.host_name = "smtp.gmail.com";
	config.server.port = 465;
	config.login.email = "littleguard.notification@gmail.com";
	config.login.password = EMAILPASSWORD;
	config.login.user_domain = "";
	config.time.ntp_server = "pool.ntp.org,time.nist.gov";
	config.time.gmt_offset = 1;
	config.time.day_light_offset = 0;

	SMTP_Message message;
	message.sender.name = "LittleGuard";
	message.sender.email = "littleguard.notification@gmail.com";
	message.subject = subject.c_str();
	message.addRecipient("User", currentSettings.emailAddress.c_str());
	message.text.content = body.c_str();
	message.text.charSet = "UTF-8";
	message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
	message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;

	if (!smtp.connect(&config)) {
		return false;
	}

	if (!MailClient.sendMail(&smtp, &message)) {
		smtp.closeSession();
		return false;
	}

	smtp.closeSession();
	return true;
}
