// Raw measurement capture, to study the sensor signals off the board.
//
// Type 'c' in the serial monitor: the alarm task records every raw sample it
// takes (flame sensor and photoresistor, every 2 ms) for 5 s, then loop()
// prints them as CSV. Start the monitor with "-f log2file" to save them to a
// file in the project folder.

#pragma once

#include <Arduino.h>

// Starts a 5 s recording. Returns false if one is already in progress.
bool captureStart();

// Called by the alarm task for every sample. Very short, never blocks.
void captureRecord(uint32_t flameMilliVolts, uint32_t lightMilliVolts,
                   bool alarmActive);

// Prints the recording once it is complete, then gets ready for the next one.
// Call from loop(): it blocks while printing, about 5 s. The alarm task keeps
// running meanwhile.
void capturePrintIfReady();
