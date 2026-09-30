// Pin map of the station.
//
// Every GPIO used by the project is declared here and nowhere else, so this
// file and docs/wiring.md are the two places to check when rewiring.

#pragma once

#include <Arduino.h>

// Blue LED soldered on the ESP32 DevKit V1 board (active high).
const uint8_t PIN_ONBOARD_LED = 2;
