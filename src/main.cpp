// routine-station firmware
//
// Step 1: sensors and motor, locally. The motor turns one way, then the other,
// while the temperature, light and flame readings are printed every second.
// No network and no alarm yet: the flame value is only displayed.
//
// loop() never calls delay(): each task checks whether it is its turn and
// returns at once, so the motor keeps stepping while the sensors are read.

#include <Arduino.h>

#include "motor.h"
#include "sensors.h"

// Must match monitor_speed in platformio.ini
const unsigned long SERIAL_BAUD = 115200;

const unsigned long REPORT_INTERVAL_MS = 1000;

// Demo cycle of the motor: one turn forward, a pause, one turn back, a pause.
const long MOTOR_DEMO_HALF_STEPS = MOTOR_HALF_STEPS_PER_TURN;
const unsigned long MOTOR_DEMO_PAUSE_MS = 1000;

const char *motorStateName(MotorState state) {
  switch (state) {
    case MotorState::Forward:
      return "forward";
    case MotorState::Backward:
      return "backward";
    default:
      return "stopped";
  }
}

// Starts the next move of the demo once the previous one is over and the pause
// has elapsed.
void updateMotorDemo() {
  static bool wasRunning = false;
  static unsigned long stoppedAtMs = 0;
  static bool nextForward = true;

  if (motorIsRunning()) {
    wasRunning = true;
    return;
  }
  if (wasRunning) {
    wasRunning = false;
    stoppedAtMs = millis();
  }
  if (millis() - stoppedAtMs < MOTOR_DEMO_PAUSE_MS) {
    return;
  }

  motorMove(nextForward ? MOTOR_DEMO_HALF_STEPS : -MOTOR_DEMO_HALF_STEPS);
  nextForward = !nextForward;
}

// Prints one line of readings every REPORT_INTERVAL_MS.
void reportIfDue() {
  static unsigned long lastReportMs = 0;

  if (millis() - lastReportMs < REPORT_INTERVAL_MS) {
    return;
  }
  lastReportMs = millis();

  const SensorReadings readings = sensorsRead();
  Serial.printf("temp=%.1f C  light=%u %%  flame=%u mV  motor=%s\n",
                readings.temperatureC, readings.lightPercent,
                readings.flameMilliVolts, motorStateName(motorState()));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  sensorsBegin();
  motorBegin();

  Serial.println();
  Serial.println("routine-station: step 1, sensors and motor");
}

void loop() {
  motorUpdate();
  updateMotorDemo();
  reportIfDue();
}
