// routine-station firmware
//
// Step 3: connected station. On top of the real-time flame alarm of step 2
// (src/alarm.cpp), the station connects to the MQTT broker over TLS
// (src/network.cpp). It publishes its measurements every 5 s and the alarm
// events (src/messages.cpp), and carries out the commands it receives
// (src/commands.cpp), as described in docs/protocol.md. The motor only turns
// on command.
//
// Three FreeRTOS tasks share core 1, by priority: the flame alarm (10), the
// motor (5), then loop() (1). loop() handles everything that may wait: the
// network, which can block it for seconds while reconnecting, the messages,
// the serial monitor. It never calls delay() (apart from the stall
// experiment): each part checks whether it is its turn and returns at once.
// Keys typed in the serial monitor:
// - 'c' records 5 s of raw sensor samples (src/capture.cpp);
// - 's' turns the stall experiment on or off (see SIMULATED_STALL_MS);
// - 'n' simulates a network outage (see SIMULATED_OUTAGE_S).

#include <Arduino.h>
#include <esp_timer.h>

#include "alarm.h"
#include "capture.h"
#include "commands.h"
#include "messages.h"
#include "motor.h"
#include "network.h"
#include "sensors.h"

// Must match monitor_speed in platformio.ini
const unsigned long SERIAL_BAUD = 115200;

// Readings on the serial monitor, and measurements sent to the broker.
const unsigned long REPORT_INTERVAL_MS = 1000;
const unsigned long PUBLISH_INTERVAL_MS = 5000;

// Experiment of the step 2 journal: once turned on with 's', every pass of
// loop() stalls this long, like a slow network call, to check that the alarm
// still reacts within milliseconds.
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

// Prints the events sent by the alarm task, and publishes the alarm ones.
// For a raised alarm, two delays:
// - the task: from the first sample over the threshold to the outputs on;
// - loop(): how long after that same sample loop() got to it. An alarm
//   checked in loop() could not have reacted any sooner.
void handleAlarmEvents() {
  AlarmEvent event;
  while (alarmNextEvent(event)) {
    const int64_t agoUs = esp_timer_get_time() - event.detectedAtUs;
    switch (event.type) {
      case AlarmEventType::Raised:
        Serial.printf(
            ">>> ALARM  flame %+d mV, light %+d mV  reaction: task %.2f ms, "
            "loop %.2f ms\n",
            event.flameRiseMilliVolts, event.lightRiseMilliVolts,
            (event.reactedAtUs - event.detectedAtUs) / 1000.0, agoUs / 1000.0);
        break;
      case AlarmEventType::Ignored:
        Serial.printf(
            ">>> ignored, daylight changed  flame %+d mV, light %+d mV\n",
            event.flameRiseMilliVolts, event.lightRiseMilliVolts);
        continue;  // local diagnostic only, not part of the protocol
      case AlarmEventType::Cleared:
        Serial.println(">>> alarm cleared");
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
                          motorState(), uptimeSeconds(), networkTime(),
                          networkRssi()) > 0) {
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

  // The rises are the gaps with the slow reference levels of the alarm task.
  const SensorReadings readings = sensorsRead();
  const AlarmStatus alarm = alarmStatus();
  Serial.printf(
      "temp=%.1f C  light=%u %% (%+d mV)  flame %+d mV  alarm=%s  motor=%s  "
      "net=%s\n",
      readings.temperatureC, readings.lightPercent, alarm.lightRiseMilliVolts,
      alarm.flameRiseMilliVolts, alarmStateName(alarm.state),
      motorStateName(motorState()), networkStateName(networkState()));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  sensorsBegin();
  motorBegin();
  alarmBegin();
  networkBegin(onCommand);

  Serial.println();
  Serial.println("routine-station: step 3, connected station");
}

void loop() {
  handleSerialInput();
  handleAlarmEvents();
  networkUpdate();
  publishIfDue();
  reportIfDue();
  capturePrintIfReady();

  if (stallExperiment) {
    delay(SIMULATED_STALL_MS);
  }
}
