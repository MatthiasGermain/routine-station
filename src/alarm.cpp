#include "alarm.h"

#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "capture.h"
#include "motor.h"
#include "pins.h"
#include "sensors.h"

namespace {

// --- Sampling ---------------------------------------------------------------

const TickType_t SAMPLE_PERIOD_TICKS = pdMS_TO_TICKS(2);

// The flame value is averaged over 100 ms (50 samples). A flame flickers about
// 7 times per second (measured, see the step 2 journal): with a shorter
// average, each dip of the flicker would fall under the threshold and restart
// the count. The 100 ms also give the photoresistor, much slower than the
// infrared sensor, time to show a daylight change first.
const int FLAME_WINDOW = 50;
// The photoresistor value is averaged over 10 ms (5 samples), to see visible
// light changes as early as possible.
const int LIGHT_WINDOW = 5;

// Reference levels follow the sensors very slowly: each sample moves them by
// 1/5000 of the gap, a time constant of about 10 s (5000 x 2 ms).
const float REFERENCE_SMOOTHING = 1.0f / 5000;

// --- Thresholds --------------------------------------------------------------
// Chosen from 10 raw captures of the sensors (src/capture.cpp), replayed
// through this algorithm on a PC: no false alarm on the 6 captures without a
// flame (calm, shadows, hands), 3 lighter flames out of 4 detected within
// 30 to 175 ms. The fourth, a small flame at 30 cm, stays under the threshold.

// A flame is an infrared rise of at least this much over the reference.
// Measured: without a flame, the rise stays under 40 mV; a lighter aimed at
// the sensor from 20 cm gives +160 to +680 mV depending on the flame.
const int32_t FLAME_RISE_TO_RAISE_MV = 100;
// Samples in a row over the threshold before raising (20 ms).
const int SAMPLES_TO_RAISE = 10;

// The room counts as bright when the photoresistor reference is above this
// (50 % of its range). There, daylight changes are the main source of false
// alarms, and the visible light is checked. Not in the dark: a lighter would
// light up the room and hide itself.
const int32_t BRIGHT_REFERENCE_MV = 1650;

// In a bright room, a visible light change at least this big means daylight
// changed (shadow, cloud, someone moving). Measured: a lighter changes the
// photoresistor by 0 to 33 mV, a shadow by several hundred.
const int32_t LIGHT_CHANGE_MV = 100;
// After such a change, the references follow both sensors for this long and
// no alarm can start: the light has to settle first. The infrared sensor
// settles slower than the photoresistor after a shadow: with 300 ms, its
// tail still reached +135 mV; with 500 ms, under 40 mV.
const int64_t DAYLIGHT_HOLDOFF_US = 500 * 1000LL;

// Clear the alarm once the infrared rise has stayed below this lower
// threshold for CLEAR_DELAY_US. Two thresholds keep the alarm from flickering
// around a single limit.
const int32_t FLAME_RISE_TO_CLEAR_MV = 50;
const int64_t CLEAR_DELAY_US = 3000 * 1000LL;

// Length of a test asked by a command.
const int64_t TEST_DURATION_US = 3000 * 1000LL;

// --- Task -------------------------------------------------------------------

// Priority well above loop() (1), so the alarm task interrupts it at once,
// and below the system tasks. Pinned to core 1, like loop(), away from the
// Wi-Fi stack which runs on core 0.
const UBaseType_t TASK_PRIORITY = 10;
const BaseType_t TASK_CORE = 1;
const uint32_t TASK_STACK_BYTES = 4096;

// Events waiting for loop() to print them. If loop() is stuck long enough for
// the queue to fill up, the newest events are dropped: printing is not part
// of the safety.
const UBaseType_t EVENT_QUEUE_LENGTH = 8;
QueueHandle_t eventQueue = nullptr;

// Requests from loop() (test, silence), read by the task on every sample.
enum class Request : uint8_t { Test, Silence };
const UBaseType_t REQUEST_QUEUE_LENGTH = 4;
QueueHandle_t requestQueue = nullptr;

// Latest values, shared with loop().
portMUX_TYPE statusLock = portMUX_INITIALIZER_UNLOCKED;
AlarmStatus sharedStatus = {AlarmState::Off, 0, 0};

// Average of the last WINDOW samples of one sensor.
template <int WINDOW>
struct MovingAverage {
  uint32_t samples[WINDOW];
  uint32_t sum;
  int next;

  void fill(uint32_t value) {
    for (uint32_t &sample : samples) {
      sample = value;
    }
    sum = value * WINDOW;
    next = 0;
  }

  uint32_t add(uint32_t value) {
    sum = sum - samples[next] + value;
    samples[next] = value;
    next = (next + 1) % WINDOW;
    return sum / WINDOW;
  }
};

// What the alarm currently asks of the outputs.
struct Alarm {
  bool flame = false;      // flame alarm in progress
  bool test = false;       // test in progress
  bool muted = false;      // buzzer silenced by a command
  int64_t testEndsUs = 0;

  bool busy() const { return flame || test; }

  AlarmState state() const {
    if (flame) {
      return muted ? AlarmState::Silenced : AlarmState::On;
    }
    return test ? AlarmState::Test : AlarmState::Off;
  }
};

// Drives the LED, the buzzer and the motor lock from the alarm, touching them
// only when something changes.
void updateOutputs(const Alarm &alarm) {
  static bool applied = false;
  static bool led = false;
  static bool buzzer = false;

  const bool wantLed = alarm.busy();
  const bool wantBuzzer = alarm.busy() && !alarm.muted;
  if (applied && wantLed == led && wantBuzzer == buzzer) {
    return;
  }
  digitalWrite(PIN_ALARM_LED, wantLed ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, wantBuzzer ? HIGH : LOW);
  if (!applied || wantLed != led) {
    motorSetLocked(wantLed);
  }
  applied = true;
  led = wantLed;
  buzzer = wantBuzzer;
}

// Carries out the requests sent by loop(). The safety rules live here, so
// they hold whatever the caller checked.
void handleRequests(Alarm &alarm, int64_t nowUs) {
  Request request;
  while (xQueueReceive(requestQueue, &request, 0) == pdTRUE) {
    switch (request) {
      case Request::Test:
        if (!alarm.flame) {
          alarm.test = true;
          alarm.muted = false;
          alarm.testEndsUs = nowUs + TEST_DURATION_US;
        }
        break;
      case Request::Silence:
        if (alarm.busy()) {
          alarm.muted = true;
        }
        break;
    }
  }
  if (alarm.test && nowUs >= alarm.testEndsUs) {
    alarm.test = false;
    if (!alarm.flame) {
      alarm.muted = false;
    }
  }
}

void sendEvent(AlarmEventType type, int32_t flameRise, int32_t lightRise,
               int64_t detectedAtUs, int64_t reactedAtUs) {
  const AlarmEvent event = {type, flameRise, lightRise, detectedAtUs,
                            reactedAtUs};
  xQueueSend(eventQueue, &event, 0);
}

void alarmTask(void *) {
  // Watchdog: if this loop stops running for 5 s, the board reboots instead
  // of staying unprotected.
  esp_task_wdt_add(nullptr);

  MovingAverage<FLAME_WINDOW> flameAverage;
  MovingAverage<LIGHT_WINDOW> lightAverage;
  const uint32_t firstFlame = sensorsReadFlameMilliVolts();
  const uint32_t firstLight = sensorsReadLightMilliVolts();
  flameAverage.fill(firstFlame);
  lightAverage.fill(firstLight);
  float flameReference = firstFlame;
  float lightReference = firstLight;

  Alarm alarm;
  int samplesOver = 0;
  int64_t firstOverUs = 0;
  int64_t flameLastSeenUs = 0;
  int64_t holdoffUntilUs = 0;

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, SAMPLE_PERIOD_TICKS);
    esp_task_wdt_reset();

    const uint32_t flameSample = sensorsReadFlameMilliVolts();
    const uint32_t lightSample = sensorsReadLightMilliVolts();
    const int64_t nowUs = esp_timer_get_time();
    captureRecord(flameSample, lightSample, alarm.flame);
    handleRequests(alarm, nowUs);

    const uint32_t flame = flameAverage.add(flameSample);
    const uint32_t light = lightAverage.add(lightSample);

    int32_t flameRise = (int32_t)flame - (int32_t)flameReference;
    int32_t lightRise = (int32_t)light - (int32_t)lightReference;
    const bool bright = lightReference >= BRIGHT_REFERENCE_MV;

    if (bright && abs(lightRise) >= LIGHT_CHANGE_MV) {
      // The visible light moved: daylight is changing, and the infrared
      // change comes from it, not from a flame.
      if (!alarm.flame && samplesOver > 0) {
        sendEvent(AlarmEventType::Ignored, flameRise, lightRise, nowUs, nowUs);
      }
      holdoffUntilUs = nowUs + DAYLIGHT_HOLDOFF_US;
    }
    if (nowUs < holdoffUntilUs) {
      // Until the light settles, the references follow both sensors and no
      // alarm can start. This also ends an alarm that daylight would
      // otherwise keep on forever.
      flameReference = flame;
      lightReference = light;
      flameRise = 0;
      lightRise = 0;
      samplesOver = 0;
    }

    if (!alarm.flame) {
      if (flameRise >= FLAME_RISE_TO_RAISE_MV) {
        if (samplesOver == 0) {
          firstOverUs = nowUs;
        }
        samplesOver++;
      } else {
        samplesOver = 0;
        // References only follow the sensors while nothing looks like a
        // flame.
        flameReference += REFERENCE_SMOOTHING * (flame - flameReference);
        lightReference += REFERENCE_SMOOTHING * (light - lightReference);
      }

      if (samplesOver >= SAMPLES_TO_RAISE) {
        // A real alarm takes over a test, and always sounds.
        alarm.flame = true;
        alarm.test = false;
        alarm.muted = false;
        updateOutputs(alarm);
        const int64_t reactedAtUs = esp_timer_get_time();
        flameLastSeenUs = nowUs;
        sendEvent(AlarmEventType::Raised, flameRise, lightRise, firstOverUs,
                  reactedAtUs);
      }
    } else {
      // During the alarm the references are frozen: a flame that lasts must
      // not become the new normal.
      if (flameRise >= FLAME_RISE_TO_CLEAR_MV) {
        flameLastSeenUs = nowUs;
      } else if (nowUs - flameLastSeenUs >= CLEAR_DELAY_US) {
        alarm.flame = false;
        alarm.muted = false;
        samplesOver = 0;
        sendEvent(AlarmEventType::Cleared, flameRise, lightRise, nowUs, nowUs);
      }
    }

    updateOutputs(alarm);

    portENTER_CRITICAL(&statusLock);
    sharedStatus = {alarm.state(), flameRise, lightRise};
    portEXIT_CRITICAL(&statusLock);
  }
}

}  // namespace

void alarmBegin() {
  pinMode(PIN_ALARM_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  updateOutputs(Alarm());

  eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(AlarmEvent));
  requestQueue = xQueueCreate(REQUEST_QUEUE_LENGTH, sizeof(Request));
  xTaskCreatePinnedToCore(alarmTask, "alarm", TASK_STACK_BYTES, nullptr,
                          TASK_PRIORITY, nullptr, TASK_CORE);
}

AlarmStatus alarmStatus() {
  portENTER_CRITICAL(&statusLock);
  const AlarmStatus status = sharedStatus;
  portEXIT_CRITICAL(&statusLock);
  return status;
}

bool alarmNextEvent(AlarmEvent &event) {
  return eventQueue != nullptr &&
         xQueueReceive(eventQueue, &event, 0) == pdTRUE;
}

void alarmRequestTest() {
  const Request request = Request::Test;
  xQueueSend(requestQueue, &request, 0);
}

void alarmRequestSilence() {
  const Request request = Request::Silence;
  xQueueSend(requestQueue, &request, 0);
}
