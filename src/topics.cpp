#include "topics.h"
#include "device_id.h"
#include "secrets.h"

String temperatureTopic;
String motionTopic;
String lastMotionTopic;
String commandTopic;
String settingsTopic;
String streamTopic;
String snapshotTopic;
String ablyChannelName;

static String topicMac;
static String topicPrefix;

static String buildTopic(const char *suffix) {
	return topicPrefix + "/" + String(suffix);
}

void initTopics() {
	topicMac = getDeviceMac(); // lowercase, no separators

	String root = MQTT_TOPIC_ROOT;
	root.trim();
	if (root.length() == 0) {
		root = "littleguard";
	}

	topicPrefix = root + "/" + topicMac;

	temperatureTopic = buildTopic("temperature");
	motionTopic = buildTopic("motion");
	lastMotionTopic = buildTopic("last_motion");
	commandTopic = buildTopic("command");
	settingsTopic = buildTopic("settings");
	streamTopic = buildTopic("stream_control");
	snapshotTopic = buildTopic("snapshot");

	ablyChannelName = String(ABLY_CHANNEL_BASE) + "-" + topicMac;
}

String getTopicMac() {
	if (topicMac.length() == 0) {
		initTopics();
	}
	return topicMac;
}
