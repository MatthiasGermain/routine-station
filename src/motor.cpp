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

// loop() drives the motor, and the alarm task can interrupt loop() at any
// moment to stop it. Every access to the state below happens inside a
// critical section on this spinlock, so the alarm can never slip in between
// "a step is due" and "energize the coils", which would turn them back on
// right after the alarm turned them off.
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;

uint8_t phase = 0;          // current row of HALF_STEP_SEQUENCE
long remainingSteps = 0;    // half-steps left in the current move
int8_t direction = 0;       // +1 forward, -1 backward, 0 stopped
bool locked = false;        // set by the alarm: no move allowed
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

// Must be called inside the critical section.
void stopNow() {
  remainingSteps = 0;
  direction = 0;
  releaseCoils();
}

}  // namespace

void motorBegin() {
  for (uint8_t pin : COIL_PINS) {
    pinMode(pin, OUTPUT);
  }
  releaseCoils();
}

void motorMove(long halfSteps) {
  portENTER_CRITICAL(&stateLock);
  if (locked || halfSteps == 0) {
    stopNow();
  } else {
    direction = halfSteps > 0 ? 1 : -1;
    remainingSteps = labs(halfSteps);
    lastStepUs = micros();
  }
  portEXIT_CRITICAL(&stateLock);
}

void motorStop() {
  portENTER_CRITICAL(&stateLock);
  stopNow();
  portEXIT_CRITICAL(&stateLock);
}

void motorSetLocked(bool value) {
  portENTER_CRITICAL(&stateLock);
  locked = value;
  if (locked) {
    stopNow();
  }
  portEXIT_CRITICAL(&stateLock);
}

void motorUpdate() {
  const unsigned long now = micros();

  portENTER_CRITICAL(&stateLock);
  // If loop() was late, the next step simply comes later: catching up with a
  // burst of steps would be too fast for the motor.
  if (remainingSteps > 0 && now - lastStepUs >= HALF_STEP_INTERVAL_US) {
    lastStepUs = now;
    phase = (phase + direction + PHASE_COUNT) % PHASE_COUNT;
    applyPhase();

    remainingSteps--;
    if (remainingSteps == 0) {
      stopNow();
    }
  }
  portEXIT_CRITICAL(&stateLock);
}

bool motorIsRunning() {
  portENTER_CRITICAL(&stateLock);
  const bool running = remainingSteps > 0;
  portEXIT_CRITICAL(&stateLock);
  return running;
}

bool motorIsLocked() {
  portENTER_CRITICAL(&stateLock);
  const bool isLocked = locked;
  portEXIT_CRITICAL(&stateLock);
  return isLocked;
}

MotorState motorState() {
  portENTER_CRITICAL(&stateLock);
  MotorState state = MotorState::Stopped;
  if (locked) {
    state = MotorState::Locked;
  } else if (direction > 0) {
    state = MotorState::Forward;
  } else if (direction < 0) {
    state = MotorState::Backward;
  }
  portEXIT_CRITICAL(&stateLock);
  return state;
}
