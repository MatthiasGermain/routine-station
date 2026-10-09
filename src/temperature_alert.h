// Temperature alert, with a threshold set from the web (docs/protocol.md).
//
// The alert starts when the temperature stays at or above the threshold for
// 5 s, and ends when it stays 1 degree below it for 5 s: the delay and the gap
// keep the noise of the sensor from toggling it near the threshold. The
// onboard blue LED is on during the alert.
//
// A supervision alert, not a safety function: it runs from loop(), so a
// network reconnection may delay it by a few seconds, and it neither sounds
// the buzzer nor locks the motor.
//
// The threshold is kept in flash (NVS) and survives a reboot.

#pragma once

#include <Arduino.h>

// Allowed thresholds, in whole degrees Celsius.
const int TEMPERATURE_THRESHOLD_MIN_C = 10;
const int TEMPERATURE_THRESHOLD_MAX_C = 40;

struct TemperatureAlertStatus {
  int thresholdC;
  bool high;  // alert in progress
};

enum class TemperatureEventType : uint8_t { High, Normal };

struct TemperatureEvent {
  TemperatureEventType type;
  float temperatureC;  // reading that confirmed the change
  int thresholdC;
};

// Loads the threshold from flash and sets up the onboard LED. Call once from
// setup().
void temperatureAlertBegin();

// Feeds a new temperature reading, about once per second. Returns true, and
// fills `event`, when the alert starts or ends.
bool temperatureAlertUpdate(float temperatureC, TemperatureEvent &event);

// Sets and saves a new threshold. Returns false, and changes nothing, if it is
// outside TEMPERATURE_THRESHOLD_MIN_C..TEMPERATURE_THRESHOLD_MAX_C.
bool temperatureAlertSetThreshold(int thresholdC);

TemperatureAlertStatus temperatureAlertStatus();
