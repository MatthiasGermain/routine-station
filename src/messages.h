// JSON messages sent by the station, in the format of docs/protocol.md.

#pragma once

#include <Arduino.h>
#include <time.h>

#include "alarm.h"
#include "motor.h"
#include "sensors.h"

// Names used both in the messages and on the serial monitor.
const char *alarmStateName(AlarmState state);
const char *motorStateName(MotorState state);

// Each function writes its JSON into `out` and returns its length, or 0 if it
// did not fit. A time of 0 means the clock is not synchronized yet: the
// message then carries "time": null.

// routine/station/measurements
size_t messageMeasurements(char *out, size_t size,
                           const SensorReadings &readings,
                           const AlarmStatus &alarm, MotorState motor,
                           uint32_t uptimeSeconds, time_t now, int rssi);

// routine/station/events (emergency stop raised or reset)
size_t messageEvent(char *out, size_t size, const AlarmEvent &event,
                    time_t eventTime);
