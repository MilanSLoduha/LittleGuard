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

void initTopics();

String getTopicMac();
