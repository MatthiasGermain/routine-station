// 28BYJ-48 stepper motor driven through a ULN2003 board, in half-steps.
//
// The motor steps in its own FreeRTOS task, one half-step every 2 ms, so it
// keeps turning while loop() waits on the network: a reconnection to the
// broker can block loop() for several seconds.
// See docs/decisions/0002-pilote-moteur-maison.md and
// docs/decisions/0007-moteur-tache-freertos.md.
//
// Safe to call from several tasks: the flame alarm task stops the motor while
// the motor task may be in the middle of a step.

#pragma once

#include <Arduino.h>

enum class MotorState : uint8_t { Stopped, Forward, Backward, Locked };

// Sets up the four driver pins, coils off, and starts the motor task. Call
// once from setup().
void motorBegin();

// Turns continuously in one direction until motorStop(). Returns false, and
// does nothing, while the motor is locked.
bool motorRun(bool forward);

// Stops right away and turns the coils off.
void motorStop();

// Locking stops the motor at once and refuses to run until it is unlocked.
// Used by the flame alarm. Unlocking does not restart the motor.
void motorSetLocked(bool locked);

bool motorIsLocked();
MotorState motorState();
