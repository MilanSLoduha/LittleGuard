#include "device_id.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_wifi.h>

static String cachedMac;
static String cachedMacPretty;

static String readHardwareMac() {
	uint8_t mac[6] = {0};
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	char buf[13];
	snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
	         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return String(buf);
}

String getDeviceMac() {
	if (cachedMac.length() == 12) {
		return cachedMac;
	}

	Preferences prefs;
	prefs.begin("device", false);
	String stored = prefs.getString("mac", "");
	prefs.end();

	if (stored.length() == 12) {
		stored.toLowerCase();
		cachedMac = stored;
		return cachedMac;
	}

	cachedMac = readHardwareMac();
	cachedMac.toLowerCase();

	prefs.begin("device", false);
	prefs.putString("mac", cachedMac);
	prefs.end();

	return cachedMac;
}

String getDeviceMacPretty() {
	if (cachedMacPretty.length() > 0) {
		return cachedMacPretty;
	}

	String mac = getDeviceMac();
	if (mac.length() != 12) {
		cachedMacPretty = mac;
		return cachedMacPretty;
	}

	char buf[18];
	snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
	         strtoul(mac.substring(0, 2).c_str(), nullptr, 16),
	         strtoul(mac.substring(2, 4).c_str(), nullptr, 16),
	         strtoul(mac.substring(4, 6).c_str(), nullptr, 16),
	         strtoul(mac.substring(6, 8).c_str(), nullptr, 16),
	         strtoul(mac.substring(8, 10).c_str(), nullptr, 16),
	         strtoul(mac.substring(10, 12).c_str(), nullptr, 16));
	cachedMacPretty = String(buf);
	return cachedMacPretty;
}
