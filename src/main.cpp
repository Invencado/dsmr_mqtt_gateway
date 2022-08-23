/**
  This sketch introduces
  * P1 meter - using code from https://github.com/fliphess/esp8266_p1meter
  * NTP Client with support for milliseconds - https://github.com/gmag11/ESPNtpClient
  * WiFiManager to connect to networks in runtime - https://github.com/tzapu/WiFiManager
  * 
*/
#include "main.h"

WiFiManager wm; // global wm instance
WiFiClient espClient;
P1Meter p1Client;
PubSubClient client(espClient);
unsigned long LAST_UPDATE_SENT = 0;
unsigned int reconnection_attempts = 0;

void reconnect() {
  /*
   * This function is called when the WiFi client loses its connection
   * On the loss of WiFi, the controller should also reconnect to the MQTT server
   */
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("MQTT - start connecting to MQTT server");


    String path = String(MQTT_ROOT_TOPIC) + "/" + macToStr();
    //clientID
    String clientId = "P1Meter" + macToStr();

    //connection_topic
    String last_will_topic = path + "/status/connection";
    if (client.connect(clientId.c_str(), MQTT_USERNAME, "", last_will_topic.c_str(), 0, true, "{\"connected\":0}")) {
      send_status_update("on_reconnect");
      client.subscribe((path + "/cmd/request_status").c_str());
      reconnection_attempts = 0;
    } else {
      reconnection_attempts = reconnection_attempts + 1;
      Serial.print("MQTT - Number of reconnection attempts = ");
      Serial.println(reconnection_attempts);
    }
    if (reconnection_attempts > 2){
      Serial.println("MQTT - restart ESP");
      ESP.restart();
    }
  }
}

void callback(char* topic, uint8_t* payload, unsigned int length) {
  /*
   * This function is called when a message arrives via MQTT on which the client has a subscription.
   * This callback is used to send status updates back to MQTT server.
   */
  Serial.print("MQTT - Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String path = String(MQTT_ROOT_TOPIC) + "/" + macToStr();
  if (String(topic) == path + String("/cmd/request_status")) {
    send_status_update("on_mqtt_request");
  }
}

String macToStr() {
  /*
   * Function to get the macadress of the controller into a string.
   */
  unsigned char mac[6];
  WiFi.macAddress(mac);
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
}


bool publish_message(const char* topic, const char* payload, bool retain) {
  /*
   * This function publishes a message to the mqtt broker.
   * The structure of the topic is made as follows:
   *  MQTT_ROOT_TOPIC/macadres/topic_from_parameter
   */
  String topic_path = String(MQTT_ROOT_TOPIC) + "/" + macToStr() + "/" + String(topic);
  bool result = client.publish(topic_path.c_str(), payload, retain);
  return result;
}

StaticJsonDocument<200> general_status;
void send_status_update(String source) {
  //connection
  publish_message("status/connection", "{\"connected\":1}", false);

  //metadata
  String ipaddress = String(String(WiFi.localIP()[0]) + '.' + String(WiFi.localIP()[1]) + '.' + String(WiFi.localIP()[2]) + '.' + String(WiFi.localIP()[3]));
  general_status["local_ip"] = ipaddress.c_str();
  general_status["rssi"] = String(WiFi.RSSI());
  general_status["ssid"] = String(WiFi.SSID());
  general_status["source"] = source.c_str();
  general_status["time"] = String(NTP.micros());
  general_status["uptime"] = String(NTP.getUptime());
  String status_output = "";
  serializeJson(general_status, status_output);
  publish_message("status/meta", status_output.c_str(), false);
}

// **********************************
// * Setup WifiManager *
// **********************************
void setup_wifimanager() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wm.setMenu(menu);
  //wm.resetSettings(); // wipe settings
  wm.setClass("invert");
  wm.setConfigPortalTimeout(600); // auto close configportal after n seconds & restart the ESP

  String accesspoint_name = String("P1Meter_") + macToStr();
  bool res = wm.autoConnect(accesspoint_name.c_str(), "password"); // password protected ap

  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi
    Serial.println("Connected to the WiFi network");
  }
}

// **********************************
// * Setup MQTT Client *
// **********************************
void setup_mqttclient() {
  //MQTT client
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  reconnect();
}

// **********************************
// * Setup NTP time synchronisation*
// **********************************

boolean syncEventTriggered = false; // True if a time even has been triggered
NTPEvent_t ntpEvent; // Last triggered event
void setup_ntp()
{
  NTP.setTimeZone (TZ_Etc_UTC);
  NTP.begin("europe.pool.ntp.org",false);
  NTP.onNTPSyncEvent ([] (NTPEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });
}



void setup() {
  Serial.println("Setup - started");
  setup_wifimanager();
  Serial.println("Setup - WiFi manager is running");
  
  setup_ntp();
  //Wait until the NTP server is synced
  unsigned long retries = 0;
  while (NTP.getLastNTPSync()==0) {
    retries += 1;
    if(10000000 <= retries){
      Serial.println("NTP - no fix found, restart");
      ESP.restart();
    }
    yield();
  }
  Serial.println("Setup - NTP client is running");
  
  setup_mqttclient();
  Serial.println("Setup - MQTT client is running");
  pinMode(LED, OUTPUT); // Initialize the LED pin as an output
  Serial.println("Setup - finished");
}

// **********************************
// * P1                             *
// **********************************

void send_data_to_broker(StaticJsonDocument<245> counters, StaticJsonDocument<245> actual_values){
  /*
   * Messages are split up to keep beneath the 256 byte size limit of the messages.
   * All messages get the same timestamp so that telegram can be put together based on this timestamp afterwards
   */

  String timestamp = String(NTP.micros());
  //data
  String counters_output = "";
  counters["time"] = timestamp;
  serializeJson(counters, counters_output);
  publish_message("data/counters", counters_output.c_str(), false);

 
  String actual_values_output = "";
  actual_values["time"] = timestamp;
  serializeJson(actual_values, actual_values_output);
  publish_message("data/actual_values", actual_values_output.c_str(), false);
}

StaticJsonDocument<245> counters;
StaticJsonDocument<245> actual_values;

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() - LAST_UPDATE_SENT > UPDATE_INTERVAL) {
        bool result = p1Client.read_p1_hardwareserial();
        if (result){
          counters = p1Client.get_counters();
          actual_values = p1Client.get_actual_values();
          send_data_to_broker(counters, actual_values);
          LAST_UPDATE_SENT = millis();
          digitalWrite(LED, !digitalRead(LED));
        }  
  }
}