// Network: Wi-Fi, then MQTT over TLS to the broker, as described in
// docs/protocol.md.
//
// Reconnects by itself: the Wi-Fi driver retries on its own, and the broker
// connection is retried with growing delays (2 s, 4 s, 8 s... up to 60 s).
// loop() is only blocked while connecting to the broker (a few seconds, under
// 30 s when the network does not answer): the alarm and the motor run
// in their own tasks and do not depend on it.
// See docs/decisions/0006-pubsubclient.md.

#pragma once

#include <Arduino.h>
#include <time.h>

enum class NetworkState : uint8_t { WifiDown, BrokerDown, Online };

// Command messages received on routine/station/commands.
using CommandHandler = void (*)(const uint8_t *payload, size_t length);

// Starts the Wi-Fi and the clock synchronization. Call once from setup().
void networkBegin(CommandHandler onCommand);

// Keeps the connections alive and handles incoming messages. Call on every
// pass of loop().
void networkUpdate();

// Test helper: turns the Wi-Fi off for `seconds`, then lets the station
// reconnect by itself, to check the behavior during an outage.
void networkSimulateOutage(uint32_t seconds);

NetworkState networkState();

// Wi-Fi signal strength in dBm (0 when not connected).
int networkRssi();

// Current UTC time, or 0 while the clock is not synchronized yet (NTP).
time_t networkTime();

// Publications, each to its own topic. They return false, and send nothing,
// while the broker is unreachable, except events, which are kept (up to 8)
// and sent as soon as the connection comes back.
bool networkPublishMeasurements(const char *json);
void networkPublishEvent(const char *json);
bool networkPublishReply(const char *json);
