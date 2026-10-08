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
const TickType_t STEP_PERIOD_TICKS = pdMS_TO_TICKS(2);

// Priority above loop() (1), so a blocked loop() does not stop the motor, and
// below the alarm task (10), which must always come first. Pinned to core 1,
// like them.
const UBaseType_t TASK_PRIORITY = 5;
const BaseType_t TASK_CORE = 1;
const uint32_t TASK_STACK_BYTES = 2048;

// The motor task steps the motor, loop() starts and stops it on command, and
// the alarm task can lock it at any moment. Every access to the state below
// happens inside a critical section on this spinlock, so the alarm can never
// slip in between "the motor is running" and "energize the coils", which
// would turn them back on right after the alarm turned them off.
portMUX_TYPE stateLock = portMUX_INITIALIZER_UNLOCKED;

uint8_t phase = 0;      // current row of HALF_STEP_SEQUENCE
int8_t direction = 0;   // +1 forward, -1 backward, 0 stopped
bool locked = false;    // set by the alarm: running not allowed

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
  direction = 0;
  releaseCoils();
}

void motorTask(void *) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, STEP_PERIOD_TICKS);

    portENTER_CRITICAL(&stateLock);
    if (direction != 0) {
      phase = (phase + direction + PHASE_COUNT) % PHASE_COUNT;
      applyPhase();
    }
    portEXIT_CRITICAL(&stateLock);
  }
}

}  // namespace

void motorBegin() {
  for (uint8_t pin : COIL_PINS) {
    pinMode(pin, OUTPUT);
  }
  releaseCoils();
  xTaskCreatePinnedToCore(motorTask, "motor", TASK_STACK_BYTES, nullptr,
                          TASK_PRIORITY, nullptr, TASK_CORE);
}

bool motorRun(bool forward) {
  portENTER_CRITICAL(&stateLock);
  const bool allowed = !locked;
  if (allowed) {
    direction = forward ? 1 : -1;
  }
  portEXIT_CRITICAL(&stateLock);
  return allowed;
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
