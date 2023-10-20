#include <ESP8266WiFi.h>
#include <ESPPubSubClientWrapper.h>

#include "ir.h"

#include "route.h"

#include "config.h"

ESPPubSubClientWrapper client(MQTT_SERVER);
String MAC_ADDR;

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
  client.publish("init ", "esp8266 connected successfully");
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
  MAC_ADDR = WiFi.macAddress();
  MAC_ADDR.replace(":", "");
  Serial.print(MAC_ADDR);

  /**
   * MQTT
  */
  StaticJsonDocument<256> doc;
  doc["ID"] = MAC_ADDR;

  char buf[256];
  serializeJson(doc, buf, 256);

  client.setCallback(callback);
  client.onConnect(connectSuccess);
  client.setBufferSize(IR_MSG_BUFFER_SIZE);
  // change the topic to pre defined micro
  client.on("lightOff", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, HIGH);});
  client.on("lightOn", [](char* topic, byte* payload, unsigned int length) {digitalWrite(LED_BUILTIN, LOW);});
  client.on(("dump/" + MAC_ADDR + "/start").c_str(), [](char* topic, byte* payload, unsigned int length) {
    irrecv.enableIRIn();
  });
  client.on(("dump/" + MAC_ADDR + "/stop").c_str(), [](char* topic, byte* payload, unsigned int length) {
    irrecv.disableIRIn();
  });
  client.on("dev/discover", [&](char* topic, byte* payload, unsigned int length) {
    client.publish("dev", buf);
  });
  
  client.on(("sendraw/" + MAC_ADDR).c_str(), toJson([](char* topic, auto payload) {

    JsonArray arr = payload["rawData"];
    uint16_t *data = new uint16_t[arr.size()];

    for (int i = 0; i < arr.size(); i++) {
      data[i] = arr[i].as<uint16_t>();
    }

    irsend.sendRaw(data, arr.size(), 38);

    delete[] data;
  }));

  client.on(("sendac/" + MAC_ADDR + "/Daikin").c_str(),toJson([](char* topic, auto payload) {

    IRDaikinESP ac(kIrLed);

    // Set up what we want to send. See ir_Daikin.cpp for all the options.
    ac.on();
    ac.setFan(1);
    ac.setMode(kDaikinCool);
    ac.setTemp(25);
    ac.setSwingVertical(false);
    ac.setSwingHorizontal(false);

    // Set the current time to 1:33PM (13:33)
    // Time works in minutes past midnight
    ac.setCurrentTime(13 * 60 + 33);
    // Turn off about 1 hour later at 2:30PM (14:30)
    ac.enableOffTimer(14 * 60 + 30);

    // Display what we are going to send.
    Serial.println(ac.toString());

    // Now send the IR signal.
  #if SEND_DAIKIN
    ac.send();
  #endif  // SEND_DAIKIN

  }));

  client.publish("dev", buf);
  client.on("disconnect", [](char* topic, byte* payload, unsigned int length) {client.disconnect();});

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

    String irMsg;

    // should be wrapped into a func inside a library
    DynamicJsonDocument doc(DOC_BUFFER_LIMIT);
    
    doc["protocol"] = typeToString(results.decode_type);
    doc["code"] = resultToHexidecimal(&results);
    doc["length"] = results.bits;
    uint16_t *rawArr = resultToRawArray(&results);
    for(int i = 0; i < getCorrectedRawLength(&results); i++) {
      doc["rawData"][i] = rawArr[i];
    }

    if (doc["protocol"] == "NEC") {
      doc["address"] = results.address;
      doc["command"] = results.command;
    }

    delete[] rawArr;
    serializeJson(doc, irMsg);
    
    Serial.print("publish(): ");
    Serial.println(client.publish("ir/rcev", irMsg.c_str()));
  }
  // OTAloopHandler();

  /**
   * MQTT
  */
  client.loop();
}
