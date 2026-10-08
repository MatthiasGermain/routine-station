// Commands received over MQTT, validated then carried out, as described in
// docs/protocol.md.
//
// The station checks every command itself, even though the website's API
// route has already checked it: anything not explicitly allowed is refused.

#pragma once

#include <Arduino.h>

// Handles one message from routine/station/commands and writes the JSON
// reply, to publish on routine/station/replies, into `reply`.
void commandsHandle(const uint8_t *payload, size_t length, char *reply,
                    size_t replySize);
