#include "temperature_alert.h"

#include <Preferences.h>

#include "pins.h"

namespace {

// Threshold before any command was ever received.
const int DEFAULT_THRESHOLD_C = 30;

// The alert ends this far below the threshold.
const float HYSTERESIS_C = 1.0;

// A change (start or end of the alert) must hold this long to be confirmed.
const unsigned long CONFIRM_MS = 5000;

// Where the threshold is saved in flash (keys are 15 characters at most).
const char PREFS_NAMESPACE[] = "station";
const char PREFS_THRESHOLD_KEY[] = "temp_thr";

Preferences prefs;
int thresholdC = DEFAULT_THRESHOLD_C;
bool high = false;

// A change is pending since changeSinceMs, until confirmed or cancelled.
bool changePending = false;
unsigned long changeSinceMs = 0;

bool inRange(int value) {
  return value >= TEMPERATURE_THRESHOLD_MIN_C &&
         value <= TEMPERATURE_THRESHOLD_MAX_C;
}

}  // namespace

void temperatureAlertBegin() {
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  digitalWrite(PIN_ONBOARD_LED, LOW);

  prefs.begin(PREFS_NAMESPACE, false);
  const int saved = prefs.getInt(PREFS_THRESHOLD_KEY, DEFAULT_THRESHOLD_C);
  // A value out of range can only come from an older firmware: ignore it.
  thresholdC = inRange(saved) ? saved : DEFAULT_THRESHOLD_C;
}

bool temperatureAlertUpdate(float temperatureC, TemperatureEvent &event) {
  // What would change the state: reaching the threshold while there is no
  // alert, or going back 1 degree below it during an alert.
  const bool changeSeen =
      high ? temperatureC <= thresholdC - HYSTERESIS_C
           : temperatureC >= thresholdC;

  if (!changeSeen) {
    changePending = false;  // any reading the other way starts over
    return false;
  }
  if (!changePending) {
    changePending = true;
    changeSinceMs = millis();
    return false;
  }
  if (millis() - changeSinceMs < CONFIRM_MS) {
    return false;
  }

  changePending = false;
  high = !high;
  digitalWrite(PIN_ONBOARD_LED, high ? HIGH : LOW);
  event = {high ? TemperatureEventType::High : TemperatureEventType::Normal,
           temperatureC, thresholdC};
  return true;
}

bool temperatureAlertSetThreshold(int newThresholdC) {
  if (!inRange(newThresholdC)) {
    return false;
  }
  if (newThresholdC != thresholdC) {
    thresholdC = newThresholdC;
    // Written only when it changes: flash wears out with each write.
    prefs.putInt(PREFS_THRESHOLD_KEY, thresholdC);
    // The readings now compare to a new threshold: start the check over.
    changePending = false;
  }
  return true;
}

TemperatureAlertStatus temperatureAlertStatus() {
  return {thresholdC, high};
}
