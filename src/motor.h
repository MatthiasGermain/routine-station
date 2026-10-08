// 28BYJ-48 stepper motor driven through a ULN2003 board, in half-steps.
//
// Non-blocking: motorMove() only sets a target, and motorUpdate(), called on
// every pass of loop(), does one half-step when it is time. The rest of the
// program keeps running while the motor turns.
// See docs/decisions/0002-pilote-moteur-maison.md.

#pragma once

#include <Arduino.h>

// Half-steps for one turn of the output shaft. The gearbox ratio is about
// 63.7:1, not exactly 64, so this is close to one turn but not exact.
const long MOTOR_HALF_STEPS_PER_TURN = 4096;

enum class MotorState : uint8_t { Stopped, Forward, Backward };

// Sets up the four driver pins, coils off. Call once from setup().
void motorBegin();

// Starts a move: positive is forward, negative is backward. Replaces any move
// in progress.
void motorMove(long halfSteps);

// Stops right away and turns the coils off.
void motorStop();

// Does the next half-step if it is due. Call as often as possible.
void motorUpdate();

bool motorIsRunning();
MotorState motorState();
