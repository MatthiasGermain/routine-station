#include "sensors.h"

#include "pins.h"

namespace {

// Samples averaged for each reading, to smooth out the ADC noise.
const int SAMPLES_PER_READING = 16;

// LM35: 10 mV per degree Celsius, 0 mV at 0 degrees.
const float LM35_MILLIVOLTS_PER_DEGREE = 10.0;

// Supply of the photoresistor divider: in full light, the input gets close to
// it. Used as the 100 % mark.
const uint32_t LIGHT_FULL_SCALE_MILLIVOLTS = 3300;

// Average voltage on an analog pin, in millivolts. analogReadMilliVolts()
// applies the factory calibration of this chip, which raw analogRead() does
// not.
uint32_t readAverageMilliVolts(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < SAMPLES_PER_READING; i++) {
    sum += analogReadMilliVolts(pin);
  }
  return sum / SAMPLES_PER_READING;
}

}  // namespace

void sensorsBegin() {
  // The LM35 gives a small voltage: 250 mV at 25 degrees. With 0 dB
  // attenuation the ADC measures 100 to 950 mV, where it is the most accurate:
  // that is 10 to 95 degrees, plenty for a room.
  analogSetPinAttenuation(PIN_TEMPERATURE, ADC_0db);

  // The two dividers swing over the whole 0 to 3.3 V range: 11 dB attenuation
  // (the default, set here to make it explicit) covers it.
  analogSetPinAttenuation(PIN_LIGHT, ADC_11db);
  analogSetPinAttenuation(PIN_FLAME, ADC_11db);
}

SensorReadings sensorsRead() {
  SensorReadings readings;

  readings.temperatureC =
      readAverageMilliVolts(PIN_TEMPERATURE) / LM35_MILLIVOLTS_PER_DEGREE;

  // More light, lower photoresistor resistance, higher voltage on the pin.
  const uint32_t lightPercent =
      readAverageMilliVolts(PIN_LIGHT) * 100 / LIGHT_FULL_SCALE_MILLIVOLTS;
  readings.lightPercent = lightPercent > 100 ? 100 : lightPercent;

  return readings;
}

uint32_t sensorsReadFlameMilliVolts() {
  return analogReadMilliVolts(PIN_FLAME);
}

uint32_t sensorsReadLightMilliVolts() {
  return analogReadMilliVolts(PIN_LIGHT);
}
