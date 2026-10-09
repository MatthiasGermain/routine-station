// Real-time alarm: an emergency stop on a touch module (TTP223).
//
// - A touch raises the alarm at once: LED and buzzer on, motor locked.
// - The alarm stays latched once the finger is lifted.
// - Only a new long touch (2 s) on the module resets it: on site, never from
//   the web, like a real emergency stop. A machine nobody can see is not
//   restarted from afar.
//
// The touch module's output triggers a hardware interrupt, which wakes up a
// FreeRTOS task with a higher priority than loop(): the reaction does not
// depend on what loop() is doing (from step 3 on, it can wait seconds on the
// network). See docs/decisions/0003-alarme-tache-freertos.md and
// docs/decisions/0008-arret-urgence-tactile.md.
//
// The task never prints, because writing to the serial port can block. It
// sends events through a queue instead, and loop() prints them.

#pragma once

#include <Arduino.h>

enum class AlarmState : uint8_t {
  Off,       // no alarm
  On,        // emergency stop: LED and buzzer on, motor locked
  Silenced,  // emergency stop, buzzer muted by a command: LED on, motor locked
  Test,      // test asked by a command: LED, buzzer and motor lock for 3 s
};

struct AlarmStatus {
  AlarmState state;
  bool touched;  // finger on the touch module right now
};

enum class AlarmEventType : uint8_t {
  Raised,   // emergency stop: LED, buzzer, motor locked
  Cleared,  // reset by a long touch: back to normal
};

struct AlarmEvent {
  AlarmEventType type;
  int64_t detectedAtUs;  // Raised: the touch interrupt
  int64_t reactedAtUs;   // Raised: LED, buzzer and motor lock all applied
};

// Sets up the outputs, the touch interrupt and the alarm task. Call once from
// setup(), after motorBegin().
void alarmBegin();

// Latest state seen by the alarm task, for display.
AlarmStatus alarmStatus();

// Takes the next alarm event, if there is one. Never blocks: meant to be
// polled from loop().
bool alarmNextEvent(AlarmEvent &event);

// Requests from the commands received over the network, carried out by the
// alarm task within 10 ms. The safety rules of docs/protocol.md are enforced
// here, whatever the caller checked before:
// - a test lights the LED, sounds the buzzer and locks the motor for 3 s; it
//   is ignored during an emergency stop;
// - silencing only mutes the buzzer of the current alarm or test: the LED
//   stays on and the motor stays locked.
// Nothing from the network can reset an emergency stop.
void alarmRequestTest();
void alarmRequestSilence();
