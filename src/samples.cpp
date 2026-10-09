#include "samples.h"

namespace {

const time_t SLOT_SECONDS = 5 * 60;

// A slot with fewer readings than this is dropped: it started less than a
// minute before its end (boot, clock just synchronized), so its average would
// not stand for the 5 minutes.
const int MIN_READINGS = 60;

time_t currentSlot = 0;  // start of the slot being averaged, 0 before the first
float temperatureSum = 0;
uint32_t lightSum = 0;
int readingCount = 0;

}  // namespace

bool samplesAdd(const SensorReadings &readings, time_t now, Sample &sample) {
  if (now == 0) {
    return false;  // no time yet: nothing can be stored
  }

  bool slotOver = false;
  const time_t slot = now - now % SLOT_SECONDS;
  if (slot != currentSlot) {
    if (readingCount >= MIN_READINGS) {
      sample.time = currentSlot;
      sample.temperatureC = temperatureSum / readingCount;
      // Rounded to the nearest percent
      sample.lightPercent = (lightSum + readingCount / 2) / readingCount;
      slotOver = true;
    }
    currentSlot = slot;
    temperatureSum = 0;
    lightSum = 0;
    readingCount = 0;
  }

  temperatureSum += readings.temperatureC;
  lightSum += readings.lightPercent;
  readingCount++;
  return slotOver;
}
