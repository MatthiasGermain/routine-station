// routine-station firmware
//
// Step 0: toolchain check. Blinks the on-board LED and prints its state on the
// serial port, to prove that compiling, flashing and the serial monitor work.

#include <Arduino.h>

#include "pins.h"

// Must match monitor_speed in platformio.ini
const unsigned long SERIAL_BAUD = 115200;

const unsigned long BLINK_INTERVAL_MS = 500;

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_ONBOARD_LED, OUTPUT);

  Serial.println();
  Serial.println("routine-station: step 0, toolchain check");
}

void loop() {
  static bool ledOn = false;

  ledOn = !ledOn;
  digitalWrite(PIN_ONBOARD_LED, ledOn ? HIGH : LOW);
  Serial.println(ledOn ? "LED on" : "LED off");

  delay(BLINK_INTERVAL_MS);
}
