#pragma once

#include <Arduino.h>

extern String temperatureTopic;
extern String motionTopic;
extern String lastMotionTopic;
extern String commandTopic;
extern String settingsTopic;
extern String streamTopic;
extern String snapshotTopic;
extern String ablyChannelName;

// Initializes topic strings using stored device MAC; call once in setup
void initTopics();

// Returns lowercase MAC used for topics
String getTopicMac();
