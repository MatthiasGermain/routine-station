// Sensors of the station: temperature, ambient light and flame.
//
// Readings are averaged over several samples and converted with the ADC
// calibration burnt into the chip, so the values are in real units.

#pragma once

#include <Arduino.h>

struct SensorReadings {
  float temperatureC;        // LM35, in degrees Celsius
  uint8_t lightPercent;      // 0 = dark, 100 = full light (relative, not lux)
  uint16_t flameMilliVolts;  // raw sensor voltage: the more infrared, the higher
};

// Sets up the ADC inputs. Call once from setup().
void sensorsBegin();

// Reads the three sensors. Takes a few milliseconds.
SensorReadings sensorsRead();
