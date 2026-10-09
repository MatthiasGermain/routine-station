// Pin map of the station.
//
// Every GPIO used by the project is declared here and nowhere else, so this
// file and docs/wiring.md are the two places to check when rewiring.

#pragma once

#include <Arduino.h>

// Blue LED soldered on the ESP32 DevKit V1 board (active high).
const uint8_t PIN_ONBOARD_LED = 2;

// Analog sensors. All of them are on ADC1 (GPIO 32 to 39), because ADC2 stops
// working as soon as Wi-Fi is on.
const uint8_t PIN_TEMPERATURE = 34;  // LM35 output (input-only pin)
const uint8_t PIN_LIGHT = 35;        // photoresistor divider (input-only pin)

// Emergency stop: output of the TTP223 touch module, high while touched.
// Triggers a hardware interrupt.
const uint8_t PIN_TOUCH = 32;

// Stepper motor, through the ULN2003 driver inputs IN1 to IN4.
// GPIO 5 is skipped on purpose: it is a strapping pin that outputs a signal at
// boot, which would make the motor twitch.
const uint8_t PIN_MOTOR_IN1 = 19;
const uint8_t PIN_MOTOR_IN2 = 18;
const uint8_t PIN_MOTOR_IN3 = 17;  // labelled TX2 on the board
const uint8_t PIN_MOTOR_IN4 = 16;  // labelled RX2 on the board

// Alarm outputs.
const uint8_t PIN_ALARM_LED = 26;  // red LED through a 220 ohm resistor
const uint8_t PIN_BUZZER = 27;     // active buzzer, driven directly (loud
                                   // enough at 3.3 V)
