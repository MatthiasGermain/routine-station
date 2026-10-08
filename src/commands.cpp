#include "commands.h"

#include <ArduinoJson.h>

#include "alarm.h"
#include "motor.h"

namespace {

const size_t MAX_COMMAND_BYTES = 256;
const size_t MAX_ID_LENGTH = 32;

// {"id": ..., "ok": true} or {"id": ..., "ok": false, "error": ...}
void writeReply(char *reply, size_t size, const char *id, const char *error) {
  JsonDocument doc;
  if (id != nullptr) {
    doc["id"] = id;
  } else {
    doc["id"] = nullptr;
  }
  doc["ok"] = error == nullptr;
  if (error != nullptr) {
    doc["error"] = error;
  }
  serializeJson(doc, reply, size);
}

// Carries out a command whose id is valid. Returns nullptr on success, or the
// error code of the reply.
//
// Every accepted command has exactly the fields listed in docs/protocol.md:
// "id", "type", "action", plus "direction" for motor start. Checking the
// number of fields refuses any extra one.
const char *execute(JsonObjectConst command) {
  const char *type = command["type"].as<const char *>();
  const char *action = command["action"].as<const char *>();
  if (type == nullptr || action == nullptr) {
    return "invalid_command";
  }
  const AlarmState alarm = alarmStatus().state;

  if (strcmp(type, "motor") == 0) {
    if (strcmp(action, "start") == 0) {
      const char *direction = command["direction"].as<const char *>();
      if (command.size() != 4 || direction == nullptr) {
        return "invalid_command";
      }
      const bool forward = strcmp(direction, "forward") == 0;
      if (!forward && strcmp(direction, "backward") != 0) {
        return "invalid_command";
      }
      // Safety: no motor during an alarm or a test. motorRun() refuses too
      // if the alarm locked the motor in the meantime.
      if (alarm != AlarmState::Off || !motorRun(forward)) {
        return "alarm_active";
      }
      return nullptr;
    }
    if (strcmp(action, "stop") == 0) {
      if (command.size() != 3) {
        return "invalid_command";
      }
      motorStop();
      return nullptr;
    }
    return "invalid_command";
  }

  if (strcmp(type, "alarm") == 0) {
    if (command.size() != 3) {
      return "invalid_command";
    }
    if (strcmp(action, "test") == 0) {
      if (alarm == AlarmState::On || alarm == AlarmState::Silenced) {
        return "alarm_active";
      }
      alarmRequestTest();
      return nullptr;
    }
    if (strcmp(action, "silence") == 0) {
      if (alarm == AlarmState::Off) {
        return "nothing_to_silence";
      }
      alarmRequestSilence();
      return nullptr;
    }
    return "invalid_command";
  }

  return "invalid_command";
}

}  // namespace

void commandsHandle(const uint8_t *payload, size_t length, char *reply,
                    size_t replySize) {
  if (length > MAX_COMMAND_BYTES) {
    writeReply(reply, replySize, nullptr, "too_long");
    return;
  }

  // Parsing from a const pointer makes ArduinoJson copy the strings: the
  // payload buffer can be reused afterwards (PubSubClient reuses it to send
  // the reply).
  JsonDocument doc;
  const DeserializationError error =
      deserializeJson(doc, reinterpret_cast<const char *>(payload), length);
  if (error || !doc.is<JsonObjectConst>()) {
    writeReply(reply, replySize, nullptr, "invalid_json");
    return;
  }
  const JsonObjectConst command = doc.as<JsonObjectConst>();

  const char *id = command["id"].as<const char *>();
  if (id == nullptr || strlen(id) == 0 || strlen(id) > MAX_ID_LENGTH) {
    writeReply(reply, replySize, nullptr, "invalid_id");
    return;
  }

  writeReply(reply, replySize, id, execute(command));
}
