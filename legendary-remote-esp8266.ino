#include <ESP8266WiFi.h>
#include <ESPPubSubClientWrapper.h>
#include <functional>

#include "config.h"

typedef std::function<void*(char* topic, byte* payload, unsigned int length)> Callback;

ESPPubSubClientWrapper client(MQTT_SERVER);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println(topic);
  char msg[256];
  snprintf(msg, ++length, (char *)payload);
  Serial.print("payload: ");
  Serial.println(msg);
  
}

void connectSuccess(uint16_t count) {
  Serial.println("Connected to MQTT-Broker!\nThis is connection nb: ");
  Serial.println(count);
  client.publish("outTopic", "hello world");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("Connecting Wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connected ");
  Serial.println(WIFI_SSID);

  client.setCallback(callback);
  client.onConnect(connectSuccess);
  client.on("lightOff", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, HIGH);});
  client.on("lightOn", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, LOW);});
  client.on("disconnect", [](char* topic, byte* payload, unsigned int length) {client.disconnect();});
  client.subscribe("inTopic");


}

void loop() {
  client.loop();
}
