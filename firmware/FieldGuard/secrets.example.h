#pragma once
// Copy to secrets.h (git-ignored) and fill in.
#define WIFI_SSID "YourHotspotOrRouter"      // e.g. the supervisor's phone hotspot
#define WIFI_PASS "password"
#define AP_PASSWORD "fieldguard"             // local fallback access point (>= 8 chars)

#define MQTT_HOST "xxxxxxxx.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "fieldguard"
#define MQTT_PASS "change-me"
#define MQTT_TLS 1                           // 1 = TLS (8883), 0 = plain (1883)
// #define MQTT_CA_CERT "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"
// Without MQTT_CA_CERT the prototype uses setInsecure() (no server verification).
