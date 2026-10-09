// Long-term history: every 5 minutes, the average temperature and light of
// those 5 minutes, published on routine/station/samples. The broker forwards
// each sample to the website, which stores it in a database (see
// docs/protocol.md). The station keeps nothing itself, apart from the samples
// waiting for the network (src/network.cpp).
//
// Slots follow the clock (10:00, 10:05, 10:10...), so samples line up with the
// hours and days of the charts. No sample before the clock is synchronized: a
// sample without a time could not be stored.

#pragma once

#include <Arduino.h>
#include <time.h>

#include "sensors.h"

struct Sample {
  time_t time;  // start of the 5-minute slot, UTC
  float temperatureC;
  uint8_t lightPercent;
};

// Feeds one reading, about once per second, with the current UTC time (0 while
// the clock is not synchronized). Returns true, and fills `sample`, when a
// slot is over.
bool samplesAdd(const SensorReadings &readings, time_t now, Sample &sample);
