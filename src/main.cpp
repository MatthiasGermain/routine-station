// routine-station firmware
//
// Connected station. A real-time alarm, an emergency stop on a touch module
// (src/alarm.cpp), locks the motor within microseconds of a touch. The station
// connects to the MQTT broker over TLS (src/network.cpp), publishes its
// measurements every 5 s and the alarm events (src/messages.cpp), and carries
// out the commands it receives (src/commands.cpp), as described in
// docs/protocol.md. The motor only turns on command. A temperature alert,
// with a threshold set from the web, lights the onboard LED
// (src/temperature_alert.cpp). Every 5 minutes, the average readings go out
// for the long-term history (src/samples.cpp).
//
// Three FreeRTOS tasks share core 1, by priority: the alarm (10), the motor
// (5), then loop() (1). loop() handles everything that may wait: the network,
// which can block it for seconds while reconnecting, the messages, the serial
// monitor. It never calls delay() (apart from the stall experiment): each part
// checks whether it is its turn and returns at once. Keys typed in the serial
// monitor:
// - 's' turns the stall experiment on or off (see SIMULATED_STALL_MS);
// - 'n' simulates a network outage (see SIMULATED_OUTAGE_S).

#include <Arduino.h>
#include <esp_timer.h>

#include "alarm.h"
#include "commands.h"
#include "messages.h"
#include "motor.h"
#include "network.h"
#include "samples.h"
#include "sensors.h"
#include "temperature_alert.h"

// Must match monitor_speed in platformio.ini
const unsigned long SERIAL_BAUD = 115200;

// Readings on the serial monitor, and measurements sent to the broker.
const unsigned long REPORT_INTERVAL_MS = 1000;
// One reading per second feeds the temperature alert and the history samples.
const unsigned long READING_INTERVAL_MS = 1000;
const unsigned long PUBLISH_INTERVAL_MS = 5000;

// Experiment of the step 2 journal: once turned on with 's', every pass of
// loop() stalls this long, like a slow network call, to check that the alarm
// does not depend on it.
const unsigned long SIMULATED_STALL_MS = 500;
bool stallExperiment = false;

// Network outage simulated with 'n': longer than the 45 s the broker waits
// before publishing the Last Will.
const uint32_t SIMULATED_OUTAGE_S = 60;

const char *networkStateName(NetworkState state) {
  switch (state) {
    case NetworkState::Online:
      return "online";
    case NetworkState::BrokerDown:
      return "broker-down";
    default:
      return "wifi-down";
  }
}

uint32_t uptimeSeconds() {
  return esp_timer_get_time() / 1000000;
}

// A command arrived from the broker (called from networkUpdate()).
void onCommand(const uint8_t *payload, size_t length) {
  char reply[128];
  commandsHandle(payload, length, reply, sizeof(reply));
  Serial.printf(">>> command %.*s -> %s\n", (int)length,
                reinterpret_cast<const char *>(payload), reply);
  networkPublishReply(reply);
}

// Prints the events sent by the alarm task, and publishes them.
// For a raised alarm, two delays:
// - the task: from the touch interrupt to the outputs on;
// - loop(): how long after that same interrupt loop() got to it. An alarm
//   checked in loop() could not have reacted any sooner.
void handleAlarmEvents() {
  AlarmEvent event;
  while (alarmNextEvent(event)) {
    const int64_t agoUs = esp_timer_get_time() - event.detectedAtUs;
    switch (event.type) {
      case AlarmEventType::Raised:
        Serial.printf(
            ">>> EMERGENCY STOP  reaction: task %lld us, loop %.2f ms\n",
            event.reactedAtUs - event.detectedAtUs, agoUs / 1000.0);
        break;
      case AlarmEventType::Cleared:
        Serial.println(">>> emergency stop reset (long touch)");
        break;
    }

    // Time of the event itself: now, minus how long ago it happened.
    const time_t now = networkTime();
    const time_t eventTime = now == 0 ? 0 : now - agoUs / 1000000;
    char json[192];
    if (messageEvent(json, sizeof(json), event, eventTime) > 0) {
      networkPublishEvent(json);
    }
  }
}

// Feeds the temperature alert, and prints and publishes its start and end.
void handleTemperatureAlert(float temperatureC) {
  TemperatureEvent event;
  if (!temperatureAlertUpdate(temperatureC, event)) {
    return;
  }
  Serial.printf(">>> temperature %s: %.1f C, threshold %d C\n",
                event.type == TemperatureEventType::High
                    ? "ALERT (onboard LED on)"
                    : "back to normal",
                event.temperatureC, event.thresholdC);

  char json[192];
  if (messageTemperatureEvent(json, sizeof(json), event, networkTime()) > 0) {
    networkPublishEvent(json);
  }
}

// Feeds the 5-minute averages, and prints and publishes each finished one.
void handleSamples(const SensorReadings &readings) {
  Sample sample;
  if (!samplesAdd(readings, networkTime(), sample)) {
    return;
  }
  struct tm utc;
  gmtime_r(&sample.time, &utc);
  Serial.printf(">>> history sample %02d:%02d UTC: %.1f C, %u %%\n",
                utc.tm_hour, utc.tm_min, sample.temperatureC,
                sample.lightPercent);

  char json[96];
  if (messageSample(json, sizeof(json), sample) > 0) {
    networkPublishSample(json);
  }
}

// Reads the sensors every READING_INTERVAL_MS for the temperature alert and
// the history.
void readSensorsIfDue() {
  static unsigned long lastReadingMs = 0;

  if (millis() - lastReadingMs < READING_INTERVAL_MS) {
    return;
  }
  lastReadingMs = millis();

  const SensorReadings readings = sensorsRead();
  handleTemperatureAlert(readings.temperatureC);
  handleSamples(readings);
}

// Keys typed in the serial monitor.
void handleSerialInput() {
  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 's':
        stallExperiment = !stallExperiment;
        Serial.printf(">>> stall experiment %s: loop() stalls %lu ms per pass\n",
                      stallExperiment ? "ON" : "off", SIMULATED_STALL_MS);
        break;
      case 'n':
        networkSimulateOutage(SIMULATED_OUTAGE_S);
        break;
    }
  }
}

// Sends the measurements to the broker every PUBLISH_INTERVAL_MS.
void publishIfDue() {
  static unsigned long lastPublishMs = 0;

  if (networkState() != NetworkState::Online ||
      millis() - lastPublishMs < PUBLISH_INTERVAL_MS) {
    return;
  }
  lastPublishMs = millis();

  char json[256];
  if (messageMeasurements(json, sizeof(json), sensorsRead(), alarmStatus(),
                          motorState(), temperatureAlertStatus(),
                          uptimeSeconds(), networkTime(), networkRssi()) > 0) {
    networkPublishMeasurements(json);
  }
}

// Prints one line of readings every REPORT_INTERVAL_MS.
void reportIfDue() {
  static unsigned long lastReportMs = 0;

  if (millis() - lastReportMs < REPORT_INTERVAL_MS) {
    return;
  }
  lastReportMs = millis();

  const SensorReadings readings = sensorsRead();
  const AlarmStatus alarm = alarmStatus();
  const TemperatureAlertStatus temperatureAlert = temperatureAlertStatus();
  Serial.printf(
      "temp=%.1f C (threshold %d%s)  light=%u %%  touch=%s  alarm=%s  "
      "motor=%s  net=%s\n",
      readings.temperatureC, temperatureAlert.thresholdC,
      temperatureAlert.high ? ", HIGH" : "", readings.lightPercent,
      alarm.touched ? "yes" : "no", alarmStateName(alarm.state),
      motorStateName(motorState()), networkStateName(networkState()));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  sensorsBegin();
  temperatureAlertBegin();
  motorBegin();
  alarmBegin();
  networkBegin(onCommand);

  Serial.println();
  Serial.println("routine-station: connected station, touch emergency stop");
  Serial.printf(">>> temperature alert threshold: %d C (from flash)\n",
                temperatureAlertStatus().thresholdC);
}

void loop() {
  handleSerialInput();
  handleAlarmEvents();
  readSensorsIfDue();
  networkUpdate();
  publishIfDue();
  reportIfDue();

  if (stallExperiment) {
    delay(SIMULATED_STALL_MS);
  }
}
