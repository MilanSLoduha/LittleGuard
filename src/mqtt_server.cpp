#include "mqtt_server.h"
#include "modem.h"

bool mqtt_connect_manualLTE() {
    modem.sendAT("+CMQTTDISC=0,120"); // odpoj vsetky existujuce pripojenia
    modem.waitResponse(5000);
    delay(500);
    
    modem.sendAT("+CMQTTREL=0"); // Uvolneine vsetkych zdrojov klienta
    modem.waitResponse(5000);
    delay(500);
    
    modem.sendAT("+CMQTTSTOP"); // Stop MQTT sluzby
    modem.waitResponse(5000);
    delay(2000);

    modem.sendAT("+CMQTTSTART"); // Spustenie MQTT sluzby
    if(modem.waitResponse(10000) != 1) {
        Serial.println("Failed to start MQTT service");
        return false;
    }
    delay(1000);

    modem.sendAT("+CMQTTACCQ=0,\"", client_id, "\",1"); // ziskanie MQTT klienta
    if(modem.waitResponse(5000) != 1) {
        Serial.println("Failed to acquire MQTT client");
        return false;
    }
    
    modem.sendAT("+CMQTTCFG=\"version\",0,4"); // nastav MQTT verziu 4 = 3.1.1
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to set MQTT version");
        return false;
    }

    modem.sendAT("+CSSLCFG=\"sslversion\",0,4");  // nastav SSL verziu 4 = TLS 1.2
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to set SSL version");
    }
    
    modem.sendAT("+CSSLCFG=\"enableSNI\",0,1");  // Povol Server Name Indication
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to enable SNI");
    }
  
    modem.sendAT("+CSSLCFG=\"authmode\",0,0");  // 0 = bez autentigikacie servera
    if(modem.waitResponse() != 1) {
        Serial.println("Failed to set auth mode");
    }
    
    Serial.println("Enabling SSL for MQTT...");
    modem.sendAT("+CMQTTSSLCFG=0,0");  // SSL pre MQTT - client_id=0, ssl_ctx_index=0
    if(modem.waitResponse() != 1) {
        Serial.println("SSL config failed!");
        return false;
    }
    
    modem.sendAT("+CMQTTCONNECT=0,\"tcp://", broker_host, ":", String(broker_port), 
                 "\",60,1,\"", broker_username, "\",\"", broker_password, "\"");
    
    if(modem.waitResponse(30000) != 1) {
        Serial.println(" MQTT SSL connection failed!");
        
        modem.sendAT("+CMQTTCONNECT?"); // Debug info
        modem.waitResponse(2000);
        
        return false;
    }

    return true;
}

void mqtt_callback(const char *topic, const uint8_t *payload, uint32_t len) {
  Serial.println(topic);
  for (int i = 0; i < len; i++) {
      Serial.print((char)payload[i]);
  }
}

void mqttPrepareLTE() {
    modem.sendAT("+CMQTTSTOP"); // Zrus existujuci session
    modem.waitResponse(5000);
    
    bool enableSSL = true;
    bool enableSNI = true;
    modem.mqtt_begin(enableSSL, enableSNI);
    
    modem.mqtt_set_certificate(HivemqRootCA);
}

