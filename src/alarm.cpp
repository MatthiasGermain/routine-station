#include "alarm.h"

#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "motor.h"
#include "pins.h"

namespace {

// --- Behavior ---------------------------------------------------------------

// A new touch held this long, after the finger was lifted, resets the
// emergency stop.
const int64_t RESET_HOLD_US = 2000 * 1000LL;

// Length of a test asked by a command.
const int64_t TEST_DURATION_US = 3000 * 1000LL;

// --- Task -------------------------------------------------------------------

// Priority well above loop() (1) and the motor task (5), so the alarm task
// runs as soon as the interrupt wakes it, and below the system tasks. Pinned
// to core 1, like loop(), away from the Wi-Fi stack which runs on core 0.
const UBaseType_t TASK_PRIORITY = 10;
const BaseType_t TASK_CORE = 1;
const uint32_t TASK_STACK_BYTES = 4096;
TaskHandle_t alarmTask = nullptr;

// Without an interrupt, the task still wakes up this often: to follow a long
// touch, end a test, carry out requests and feed the watchdog.
const TickType_t POLL_PERIOD_TICKS = pdMS_TO_TICKS(10);

// Events waiting for loop() to print them. If loop() is stuck long enough for
// the queue to fill up, the newest events are dropped: printing is not part
// of the safety.
const UBaseType_t EVENT_QUEUE_LENGTH = 8;
QueueHandle_t eventQueue = nullptr;

// Requests from loop() (test, silence).
enum class Request : uint8_t { Test, Silence };
const UBaseType_t REQUEST_QUEUE_LENGTH = 4;
QueueHandle_t requestQueue = nullptr;

// Latest state, shared with loop().
portMUX_TYPE statusLock = portMUX_INITIALIZER_UNLOCKED;
AlarmStatus sharedStatus = {AlarmState::Off, false};

// Time of the last touch, written by the interrupt.
volatile int64_t touchedAtUs = 0;

// What the alarm currently asks of the outputs.
struct Alarm {
  bool emergency = false;  // emergency stop latched
  bool test = false;       // test in progress
  bool muted = false;      // buzzer silenced by a command
  int64_t testEndsUs = 0;

  bool busy() const { return emergency || test; }

  AlarmState state() const {
    if (emergency) {
      return muted ? AlarmState::Silenced : AlarmState::On;
    }
    return test ? AlarmState::Test : AlarmState::Off;
  }
};

// The touch module's output goes high: note the time and wake the alarm task
// up at once. An interrupt routine must stay this short.
void IRAM_ATTR onTouch() {
  touchedAtUs = esp_timer_get_time();
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(alarmTask, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

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
        if (!alarm.emergency) {
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
    if (!alarm.emergency) {
      alarm.muted = false;
    }
  }
}

void sendEvent(AlarmEventType type, int64_t detectedAtUs, int64_t reactedAtUs) {
  const AlarmEvent event = {type, detectedAtUs, reactedAtUs};
  xQueueSend(eventQueue, &event, 0);
}

void alarmTaskLoop(void *) {
  // Watchdog: if this loop stops running for 5 s, the board reboots instead
  // of staying unprotected.
  esp_task_wdt_add(nullptr);

  Alarm alarm;
  // After raising or resetting the alarm, the touch that did it must end
  // before the module counts again: the long touch that resets the alarm
  // must not raise it again, and the touch that raised it must not start a
  // reset.
  bool waitRelease = false;
  int64_t holdStartUs = -1;  // start of the current long touch

  for (;;) {
    // Sleeps until the touch interrupt, or POLL_PERIOD_TICKS at most.
    const bool interrupted = ulTaskNotifyTake(pdTRUE, POLL_PERIOD_TICKS) > 0;
    esp_task_wdt_reset();
    const int64_t nowUs = esp_timer_get_time();
    const bool touched = digitalRead(PIN_TOUCH) == HIGH;

    handleRequests(alarm, nowUs);

    if (!touched) {
      waitRelease = false;
      holdStartUs = -1;
    }

    if (!alarm.emergency) {
      // Any touch raises the alarm, even one too short to still be seen
      // here: the interrupt caught it.
      if ((interrupted || touched) && !waitRelease) {
        // An emergency stop takes over a test, and always sounds.
        alarm.emergency = true;
        alarm.test = false;
        alarm.muted = false;
        updateOutputs(alarm);
        const int64_t reactedAtUs = esp_timer_get_time();
        sendEvent(AlarmEventType::Raised, interrupted ? touchedAtUs : nowUs,
                  reactedAtUs);
        waitRelease = touched;
      }
    } else if (touched && !waitRelease) {
      // Latched: only a new touch held RESET_HOLD_US resets it.
      if (holdStartUs < 0) {
        holdStartUs = nowUs;
      } else if (nowUs - holdStartUs >= RESET_HOLD_US) {
        alarm.emergency = false;
        alarm.muted = false;
        sendEvent(AlarmEventType::Cleared, nowUs, nowUs);
        waitRelease = true;
        holdStartUs = -1;
      }
    }

    updateOutputs(alarm);

    portENTER_CRITICAL(&statusLock);
    sharedStatus = {alarm.state(), touched};
    portEXIT_CRITICAL(&statusLock);
  }
}

}  // namespace

void alarmBegin() {
  pinMode(PIN_ALARM_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  updateOutputs(Alarm());

  // The module drives its output itself (high while touched). The pull-down
  // keeps the input low if the module is unplugged.
  pinMode(PIN_TOUCH, INPUT_PULLDOWN);

  eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(AlarmEvent));
  requestQueue = xQueueCreate(REQUEST_QUEUE_LENGTH, sizeof(Request));
  xTaskCreatePinnedToCore(alarmTaskLoop, "alarm", TASK_STACK_BYTES, nullptr,
                          TASK_PRIORITY, &alarmTask, TASK_CORE);

  // The task exists before the interrupt can wake it.
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH), onTouch, RISING);
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
