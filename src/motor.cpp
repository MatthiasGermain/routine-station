#include "motor.h"

#include "pins.h"

namespace {

const uint8_t COIL_PINS[4] = {PIN_MOTOR_IN1, PIN_MOTOR_IN2, PIN_MOTOR_IN3,
                              PIN_MOTOR_IN4};

// Half-step sequence: one coil on, then two, then the next one alone, and so
// on. Each row is the state of IN1 to IN4. Going down the table turns the
// motor forward, going up turns it backward.
const uint8_t PHASE_COUNT = 8;
const uint8_t HALF_STEP_SEQUENCE[PHASE_COUNT][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

// Time between two half-steps. Below about 1 ms the 28BYJ-48 starts missing
// steps; 2 ms gives one turn in about 8 seconds.
const unsigned long HALF_STEP_INTERVAL_US = 2000;

uint8_t phase = 0;          // current row of HALF_STEP_SEQUENCE
long remainingSteps = 0;    // half-steps left in the current move
int8_t direction = 0;       // +1 forward, -1 backward, 0 stopped
unsigned long lastStepUs = 0;

void applyPhase() {
  for (uint8_t coil = 0; coil < 4; coil++) {
    digitalWrite(COIL_PINS[coil], HALF_STEP_SEQUENCE[phase][coil]);
  }
}

// All coils off: the shaft is free, but the motor draws no current and does
// not heat up while it waits.
void releaseCoils() {
  for (uint8_t pin : COIL_PINS) {
    digitalWrite(pin, LOW);
  }
}

}  // namespace

void motorBegin() {
  for (uint8_t pin : COIL_PINS) {
    pinMode(pin, OUTPUT);
  }
  releaseCoils();
}

void motorMove(long halfSteps) {
  if (halfSteps == 0) {
    motorStop();
    return;
  }
  direction = halfSteps > 0 ? 1 : -1;
  remainingSteps = labs(halfSteps);
  lastStepUs = micros();
}

void motorStop() {
  remainingSteps = 0;
  direction = 0;
  releaseCoils();
}

void motorUpdate() {
  if (remainingSteps == 0) {
    return;
  }

  const unsigned long now = micros();
  if (now - lastStepUs < HALF_STEP_INTERVAL_US) {
    return;
  }
  // If loop() was late, the next step simply comes later: catching up with a
  // burst of steps would be too fast for the motor.
  lastStepUs = now;

  phase = (phase + direction + PHASE_COUNT) % PHASE_COUNT;
  applyPhase();

  remainingSteps--;
  if (remainingSteps == 0) {
    motorStop();
  }
}

bool motorIsRunning() {
  return remainingSteps > 0;
}

MotorState motorState() {
  if (direction > 0) {
    return MotorState::Forward;
  }
  if (direction < 0) {
    return MotorState::Backward;
  }
  return MotorState::Stopped;
}
