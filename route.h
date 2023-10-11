#include <ArduinoJson.h>

#define DOC_BUFFER_LIMIT 1024

typedef std::function<void(char*, uint8_t*, unsigned int)> MQTT_CALLBACK;
typedef std::function<void(char*, DynamicJsonDocument&)> TO_JSON_CALLBACK;

MQTT_CALLBACK toJson(TO_JSON_CALLBACK _callback) {
    return [_callback](char *_topic, uint8_t *_payload, unsigned int _length) {
        DynamicJsonDocument doc(DOC_BUFFER_LIMIT);
        deserializeJson(doc, (char *)_payload, _length);
        _callback(_topic, doc);
    };
};



