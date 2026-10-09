// Sensors of the station: temperature and ambient light.
//
// Readings are averaged over several samples and converted with the ADC
// calibration burnt into the chip, so the values are in real units.

#pragma once

#include <Arduino.h>

struct SensorReadings {
  float temperatureC;    // LM35, in degrees Celsius
  uint8_t lightPercent;  // 0 = dark, 100 = full light (relative, not lux)
};

// Sets up the ADC inputs. Call once from setup().
void sensorsBegin();

// Reads temperature and light. Takes a few milliseconds.
SensorReadings sensorsRead();
