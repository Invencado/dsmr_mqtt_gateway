#include "settings.h"
#include "P1Meter.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#define MQTT_MAX_PACKET_SIZE 1024
#include <ESPNtpClient.h> //https://github.com/gmag11/ESPNtpClient
#include <ArduinoJson.h>
#include <token.h>

void reconnect();
void send_status_update(String source);
String macToStr();
bool publish_message(const char* topic, const char* payload, bool retain);
void processLine(int len);