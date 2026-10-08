// Template of include/secrets.h.
//
// Copy this file to include/secrets.h and fill in the real values.
// secrets.h is ignored by git: real credentials are never committed.

#pragma once

// Wi-Fi network. 2.4 GHz only: the ESP32 does not do 5 GHz.
const char WIFI_SSID[] = "your-network-name";
const char WIFI_PASSWORD[] = "your-wifi-password";

// MQTT broker, from the overview page of the EMQX deployment.
const char MQTT_HOST[] = "xxxxxxxx.ala.eu-central-1.emqxsl.com";

// Credentials of the "station" user (EMQX: Access Control > Authentication).
const char MQTT_USERNAME[] = "station";
const char MQTT_PASSWORD[] = "the-station-password";
