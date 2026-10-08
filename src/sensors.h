// Sensors of the station: temperature, ambient light and flame.
//
// Readings are converted with the ADC calibration burnt into the chip, so the
// values are in real units.

#pragma once

#include <Arduino.h>

struct SensorReadings {
  float temperatureC;    // LM35, in degrees Celsius
  uint8_t lightPercent;  // 0 = dark, 100 = full light (relative, not lux)
};

// Sets up the ADC inputs. Call once from setup().
void sensorsBegin();

// Reads temperature and light, each averaged over several samples. Takes a few
// milliseconds.
SensorReadings sensorsRead();

// Single samples in millivolts, for the alarm task, which reads them every
// 2 ms and does its own averaging (see src/alarm.cpp).
// Flame sensor: the more infrared, the higher. Only the alarm task reads it.
uint32_t sensorsReadFlameMilliVolts();
// Photoresistor: the more visible light, the higher.
uint32_t sensorsReadLightMilliVolts();
