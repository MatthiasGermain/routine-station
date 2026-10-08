#include "capture.h"

#include <esp_timer.h>

namespace {

// 5 s at one sample every 2 ms. About 30 KB of RAM, out of 320 KB.
const int CAPTURE_SAMPLES = 2500;

struct Sample {
  uint32_t timeUs;  // since the first sample of the capture
  uint16_t flameMilliVolts;
  uint16_t lightMilliVolts;
  bool alarmActive;
};

Sample samples[CAPTURE_SAMPLES];

// Idle -> Recording: loop(), on a key press.
// Recording -> Ready: the alarm task, once the buffer is full.
// Ready -> Idle: loop(), once the samples are printed.
// The samples are only read while Ready, when the task no longer writes them.
enum class State : uint8_t { Idle, Recording, Ready };

portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
State state = State::Idle;
int count = 0;
int64_t startUs = 0;

}  // namespace

bool captureStart() {
  portENTER_CRITICAL(&lock);
  const bool started = state == State::Idle;
  if (started) {
    count = 0;
    state = State::Recording;
  }
  portEXIT_CRITICAL(&lock);
  return started;
}

void captureRecord(uint32_t flameMilliVolts, uint32_t lightMilliVolts,
                   bool alarmActive) {
  const int64_t nowUs = esp_timer_get_time();

  portENTER_CRITICAL(&lock);
  if (state == State::Recording) {
    if (count == 0) {
      startUs = nowUs;
    }
    samples[count] = {(uint32_t)(nowUs - startUs), (uint16_t)flameMilliVolts,
                      (uint16_t)lightMilliVolts, alarmActive};
    count++;
    if (count == CAPTURE_SAMPLES) {
      state = State::Ready;
    }
  }
  portEXIT_CRITICAL(&lock);
}

void capturePrintIfReady() {
  portENTER_CRITICAL(&lock);
  const bool ready = state == State::Ready;
  portEXIT_CRITICAL(&lock);
  if (!ready) {
    return;
  }

  Serial.printf(">>> capture: %d samples\n", CAPTURE_SAMPLES);
  Serial.println("time_ms,flame_mv,light_mv,alarm");
  for (const Sample &sample : samples) {
    Serial.printf("%.3f,%u,%u,%u\n", sample.timeUs / 1000.0,
                  sample.flameMilliVolts, sample.lightMilliVolts,
                  sample.alarmActive ? 1 : 0);
  }
  Serial.println(">>> capture end");

  portENTER_CRITICAL(&lock);
  state = State::Idle;
  portEXIT_CRITICAL(&lock);
}
