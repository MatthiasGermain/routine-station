// routine-station firmware
//
// Step 2: real-time flame alarm. A high-priority task watches the flame
// sensor and, on a flame, turns on the LED and the buzzer and locks the motor
// within milliseconds (src/alarm.cpp). loop() only runs the motor demo,
// prints the readings and the alarm events, and handles two keys typed in the
// serial monitor:
// - 'c' records 5 s of raw sensor samples (src/capture.cpp);
// - 's' turns the stall experiment on or off (see SIMULATED_STALL_MS).
//
// loop() never calls delay() (apart from the stall experiment): each task
// checks whether it is its turn and returns at once, so the motor keeps
// stepping while the sensors are read.

#include <Arduino.h>
#include <esp_timer.h>

#include "alarm.h"
#include "capture.h"
#include "motor.h"
#include "sensors.h"

// Must match monitor_speed in platformio.ini
const unsigned long SERIAL_BAUD = 115200;

const unsigned long REPORT_INTERVAL_MS = 1000;

// Demo cycle of the motor: one turn forward, a pause, one turn back, a pause.
const long MOTOR_DEMO_HALF_STEPS = MOTOR_HALF_STEPS_PER_TURN;
const unsigned long MOTOR_DEMO_PAUSE_MS = 1000;

// Experiment of the step 2 journal: once turned on with 's', every pass of
// loop() stalls this long, like a slow network call, to check that the alarm
// still reacts within milliseconds.
const unsigned long SIMULATED_STALL_MS = 500;
bool stallExperiment = false;

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

// Starts the next move of the demo once the previous one is over and the pause
// has elapsed. Does nothing while the alarm keeps the motor locked.
void updateMotorDemo() {
  static bool wasRunning = false;
  static unsigned long stoppedAtMs = 0;
  static bool nextForward = true;

  if (motorIsLocked()) {
    return;
  }
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

// Prints the events sent by the alarm task. For a raised alarm, two delays:
// - the task: from the first sample over the threshold to the outputs on;
// - loop(): how long after that same sample loop() got to it. An alarm
//   checked in loop() could not have reacted any sooner.
void printAlarmEvents() {
  AlarmEvent event;
  while (alarmNextEvent(event)) {
    switch (event.type) {
      case AlarmEventType::Raised: {
        const int64_t taskUs = event.reactedAtUs - event.detectedAtUs;
        const int64_t loopUs = esp_timer_get_time() - event.detectedAtUs;
        Serial.printf(
            ">>> ALARM  flame %+d mV, light %+d mV  reaction: task %.2f ms, "
            "loop %.2f ms\n",
            event.flameRiseMilliVolts, event.lightRiseMilliVolts,
            taskUs / 1000.0, loopUs / 1000.0);
        break;
      }
      case AlarmEventType::Ignored:
        Serial.printf(
            ">>> ignored, daylight changed  flame %+d mV, light %+d mV\n",
            event.flameRiseMilliVolts, event.lightRiseMilliVolts);
        break;
      case AlarmEventType::Cleared:
        Serial.println(">>> alarm cleared");
        break;
    }
  }
}

// Keys typed in the serial monitor.
void handleSerialInput() {
  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'c':
        if (captureStart()) {
          Serial.println(">>> capture started: 5 s of raw samples");
        } else {
          Serial.println(">>> capture already in progress");
        }
        break;
      case 's':
        stallExperiment = !stallExperiment;
        Serial.printf(">>> stall experiment %s: loop() stalls %lu ms per pass\n",
                      stallExperiment ? "ON" : "off", SIMULATED_STALL_MS);
        break;
    }
  }
}

// Prints one line of readings every REPORT_INTERVAL_MS.
void reportIfDue() {
  static unsigned long lastReportMs = 0;

  if (millis() - lastReportMs < REPORT_INTERVAL_MS) {
    return;
  }
  lastReportMs = millis();

  // The rises are the gaps with the slow reference levels of the alarm task.
  const SensorReadings readings = sensorsRead();
  const AlarmStatus alarm = alarmStatus();
  Serial.printf(
      "temp=%.1f C  light=%u %% (%+d mV)  flame %+d mV  alarm=%s  motor=%s\n",
      readings.temperatureC, readings.lightPercent, alarm.lightRiseMilliVolts,
      alarm.flameRiseMilliVolts, alarm.active ? "ON" : "off",
      motorStateName(motorState()));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  sensorsBegin();
  motorBegin();
  alarmBegin();

  Serial.println();
  Serial.println("routine-station: step 2, real-time flame alarm");
}

void loop() {
  motorUpdate();
  updateMotorDemo();
  handleSerialInput();
  printAlarmEvents();
  reportIfDue();
  capturePrintIfReady();

  if (stallExperiment) {
    delay(SIMULATED_STALL_MS);
  }
}
