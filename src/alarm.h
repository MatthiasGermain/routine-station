// Real-time flame alarm.
//
// A FreeRTOS task with a higher priority than loop() samples the flame sensor
// and the photoresistor every 2 ms. When it sees a flame, it turns on the LED
// and the buzzer and locks the motor, whatever loop() is doing. From step 3
// on, loop() will spend up to seconds waiting on the network.
// See docs/decisions/0003-alarme-tache-freertos.md.
//
// Telling a flame from daylight: the bare infrared sensor also sees daylight,
// so more daylight looks like a flame. The photoresistor sorts it out: when
// daylight changes, the visible light changes too; a flame adds infrared
// without changing the visible light of a bright room.
//
// The task never prints, because writing to the serial port can block. It
// sends events through a queue instead, and loop() prints them.

#pragma once

#include <Arduino.h>

struct AlarmStatus {
  bool active;
  // Gaps between each sensor (flame averaged over 100 ms, light over 10 ms)
  // and its reference level, which follows the sensor slowly (about 10 s).
  int32_t flameRiseMilliVolts;
  int32_t lightRiseMilliVolts;
};

enum class AlarmEventType : uint8_t {
  Raised,   // flame: LED, buzzer, motor locked
  Cleared,  // no flame for 3 s: back to normal
  Ignored,  // infrared rise rejected: the visible light changed with it
};

struct AlarmEvent {
  AlarmEventType type;
  int32_t flameRiseMilliVolts;
  int32_t lightRiseMilliVolts;
  int64_t detectedAtUs;  // Raised: first sample over the threshold
  int64_t reactedAtUs;   // Raised: LED, buzzer and motor lock all applied
};

// Sets up the outputs and starts the alarm task. Call once from setup(), after
// sensorsBegin() and motorBegin().
void alarmBegin();

// Latest values seen by the alarm task, for display.
AlarmStatus alarmStatus();

// Takes the next alarm event, if there is one. Never blocks: meant to be
// polled from loop().
bool alarmNextEvent(AlarmEvent &event);
