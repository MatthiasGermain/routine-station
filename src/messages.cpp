#include "messages.h"

#include <ArduinoJson.h>

namespace {

// ISO 8601 UTC time ("2026-10-08T19:30:05Z"), or null when unknown.
void setTime(JsonDocument &doc, time_t time) {
  if (time == 0) {
    doc["time"] = nullptr;
    return;
  }
  struct tm utc;
  gmtime_r(&time, &utc);
  char text[21];
  strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
  doc["time"] = text;  // copied by ArduinoJson
}

size_t finish(const JsonDocument &doc, char *out, size_t size) {
  const size_t length = measureJson(doc);
  if (length >= size) {
    return 0;
  }
  return serializeJson(doc, out, size);
}

}  // namespace

const char *alarmStateName(AlarmState state) {
  switch (state) {
    case AlarmState::On:
      return "on";
    case AlarmState::Silenced:
      return "silenced";
    case AlarmState::Test:
      return "test";
    default:
      return "off";
  }
}

const char *motorStateName(MotorState state) {
  switch (state) {
    case MotorState::Forward:
      return "forward";
    case MotorState::Backward:
      return "backward";
    case MotorState::Locked:
      return "locked";
    default:
      return "stopped";
  }
}

size_t messageMeasurements(char *out, size_t size,
                           const SensorReadings &readings,
                           const AlarmStatus &alarm, MotorState motor,
                           uint32_t uptimeSeconds, time_t now, int rssi) {
  JsonDocument doc;
  setTime(doc, now);
  doc["uptime_s"] = uptimeSeconds;
  doc["temperature_c"] = roundf(readings.temperatureC * 10) / 10;
  doc["light_pct"] = readings.lightPercent;
  doc["alarm"] = alarmStateName(alarm.state);
  doc["motor"] = motorStateName(motor);
  doc["rssi_dbm"] = rssi;
  return finish(doc, out, size);
}

size_t messageEvent(char *out, size_t size, const AlarmEvent &event,
                    time_t eventTime) {
  JsonDocument doc;
  setTime(doc, eventTime);
  if (event.type == AlarmEventType::Raised) {
    doc["type"] = "alarm_raised";
    doc["cause"] = "touch";
    doc["reaction_us"] = event.reactedAtUs - event.detectedAtUs;
  } else {
    doc["type"] = "alarm_cleared";
  }
  return finish(doc, out, size);
}
