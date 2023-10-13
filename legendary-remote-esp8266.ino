#include <ESP8266WiFi.h>
#include <ESPPubSubClientWrapper.h>

#include "ir.h"

#include "route.h"

#include "config.h"

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
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  while (!Serial)  // Wait for the serial connection to be establised.
    delay(50);

  /**
   * WiFi
  */
  Serial.println("Connecting Wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connected ");
  Serial.println(WIFI_SSID);

  /**
   * MQTT
  */
  client.setCallback(callback);
  client.onConnect(connectSuccess);
  client.on("lightOff", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, HIGH);});
  client.on("lightOn", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, LOW);});
  client.on("disconnect", [](char* topic, byte* payload, unsigned int length) {client.disconnect();});
  client.on("light", toJson([](char* topic, auto payload) {
    Serial.print("light: ");
    Serial.println((String)payload["msg"]);
  }));
  client.on("sendfan", toJson([](char* topic, auto payload) {
    irsend.sendSymphony(0xD81, 12, 0);
  }));
  client.on("sendspeaker", toJson([](char* topic, auto payload) {
    irsend.sendNEC(0x1E7040BF, 32, 0);
  }));
  client.subscribe("inTopic");

  /**
   * IR
  */
  // Perform a low level sanity checks that the compiler performs bit field
  // packing as we expect and Endianness is as we expect.
  if (irutils::lowLevelSanityCheck() != 0) {
    Serial.println("lowLevelSanityCheck didn't pass");
    exit(1);
  }

  Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);

  irInit();
}

void loop() {
  /**
   * IR
  */
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
    // Check if we got an IR message that was to big for our capture buffer.
    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
    // Display the library version the message was captured with.
    Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
    // Display the tolerance percentage if it has been change from the default.
    if (kTolerancePercentage != kTolerance)
      Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));
    // Display any extra A/C info if we have it.
    String description = IRAcUtils::resultAcToString(&results);
    if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
    yield();  // Feed the WDT as the text output can take a while to print.
#if LEGACY_TIMING_INFO
    // Output legacy RAW timing info of the result.
    Serial.println(resultToTimingInfo(&results));
    yield();  // Feed the WDT (again)
#endif  // LEGACY_TIMING_INFO
    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println();    // Blank line between entries
    yield();             // Feed the WDT (again)
  }
  OTAloopHandler();

  /**
   * MQTT
  */
  client.loop();
}
