#include "network.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "broker_ca.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill it in."
#endif

namespace {

// --- Broker and topics (docs/protocol.md) -----------------------------------

const uint16_t MQTT_PORT = 8883;  // MQTT over TLS
const char CLIENT_ID[] = "routine-station";

const char TOPIC_MEASUREMENTS[] = "routine/station/measurements";
const char TOPIC_EVENTS[] = "routine/station/events";
const char TOPIC_STATUS[] = "routine/station/status";
const char TOPIC_COMMANDS[] = "routine/station/commands";
const char TOPIC_REPLIES[] = "routine/station/replies";
const char TOPIC_SAMPLES[] = "routine/station/samples";

const char STATUS_ONLINE[] = "{\"online\":true}";
// Last Will: published by the broker itself if the station disappears.
const char STATUS_OFFLINE[] = "{\"online\":false}";

// --- Timing -----------------------------------------------------------------

// Delay before retrying the broker: doubles after each failure.
const unsigned long RETRY_MIN_MS = 2000;
const unsigned long RETRY_MAX_MS = 60000;

// Upper bounds on how long a connection attempt may block loop(): opening the
// connection, the TLS handshake, then the broker's answer, under 30 s in all.
// By default they add up to more than 2 minutes (30 s, 120 s, 15 s).
//
// Opening the connection: when the first packet is lost, the ESP32 sends it
// again after 3 s, then after 9 s. 12 s allows three tries; with 5 s, only
// two, and on a weak Wi-Fi link half of the attempts failed.
const uint32_t TCP_TIMEOUT_S = 12;
const unsigned long TLS_HANDSHAKE_TIMEOUT_S = 10;
const uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

// Keep-alive: after this long without hearing from the broker, the station
// pings it, and gives up if it gets no answer within the same delay. The
// broker, without news from the station for 1.5 times this (45 s), considers
// it gone and publishes its Last Will. 30 s leaves time for lost packets to be
// sent again on a weak link; with 15 s, the connection often dropped.
const uint16_t MQTT_KEEP_ALIVE_S = 30;

// Messages are small (docs/protocol.md: commands up to 256 bytes); the
// default buffer of PubSubClient (256 bytes, topic included) is too tight.
const uint16_t MQTT_BUFFER_BYTES = 512;

// If the Wi-Fi driver has not reconnected after this long, give it a nudge.
// On a weak link, joining the network can fail several times in a row: with
// 30 s between tries, the station once needed more than 3 minutes.
const unsigned long WIFI_NUDGE_MS = 10000;

// After this many network failures in a row towards the broker while the Wi-Fi
// looks connected, restart the Wi-Fi connection: the station joins the network
// again and asks the box for a new DHCP lease, with its DNS server. Once, the
// station stayed stuck for minutes on "DNS Failed", Wi-Fi up, until a reset.
const int BROKER_FAILURES_BEFORE_WIFI_RESTART = 3;

// Clock: UTC, from public NTP servers.
const char NTP_SERVER_1[] = "pool.ntp.org";
const char NTP_SERVER_2[] = "time.google.com";
// Any time before this means the clock is not synchronized yet (2024-01-01).
const time_t CLOCK_SYNCED_AFTER = 1704067200;

// --- State ------------------------------------------------------------------

WiFiClientSecure tlsClient;
PubSubClient mqtt(tlsClient);
CommandHandler commandHandler = nullptr;

NetworkState state = NetworkState::WifiDown;
unsigned long retryDelayMs = RETRY_MIN_MS;
unsigned long lastAttemptMs = 0;
bool firstAttempt = true;
unsigned long wifiDownSinceMs = 0;
unsigned long connectedAtMs = 0;
int brokerFailures = 0;  // network failures in a row, see connectBroker()

// Simulated network outage (networkSimulateOutage()).
bool outage = false;
unsigned long outageEndsMs = 0;

// Events waiting for the broker.
const int PENDING_EVENTS = 8;
const size_t EVENT_BYTES = 192;
char pendingEvents[PENDING_EVENTS][EVENT_BYTES];
int pendingEventCount = 0;

// History samples waiting for the broker: 24 of them, 2 hours of history.
const int PENDING_SAMPLES = 24;
const size_t SAMPLE_BYTES = 96;
char pendingSamples[PENDING_SAMPLES][SAMPLE_BYTES];
int pendingSampleCount = 0;

void onMessage(char *topic, uint8_t *payload, unsigned int length) {
  if (strcmp(topic, TOPIC_COMMANDS) == 0 && commandHandler != nullptr) {
    commandHandler(payload, length);
  }
}

// Publishes the messages of a waiting queue, oldest first, until the broker
// stops accepting them. Works for both queues (events and samples), whatever
// the size of their messages.
template <size_t MESSAGE_BYTES>
void flushPending(const char *topic, char (*queue)[MESSAGE_BYTES], int &count) {
  int sent = 0;
  while (sent < count && mqtt.publish(topic, queue[sent])) {
    sent++;
  }
  // Keep the ones that did not go out, in order.
  for (int i = sent; i < count; i++) {
    strcpy(queue[i - sent], queue[i]);
  }
  count -= sent;
}

void connectBroker() {
  Serial.printf(">>> MQTT: connecting to %s:%u...\n", MQTT_HOST, MQTT_PORT);
  const unsigned long startMs = millis();
  const bool connected =
      mqtt.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, TOPIC_STATUS, 1,
                   true, STATUS_OFFLINE);
  const unsigned long durationMs = millis() - startMs;

  if (!connected) {
    // state() < 0: network or TLS failure; > 0: refused by the broker
    // (4 or 5: wrong username or password).
    retryDelayMs = min(retryDelayMs * 2, RETRY_MAX_MS);
    Serial.printf(">>> MQTT: failed after %lu ms (state %d), next try in %lu s\n",
                  durationMs, mqtt.state(), retryDelayMs / 1000);

    // A refusal by the broker proves the network works: only network
    // failures count towards a Wi-Fi restart.
    if (mqtt.state() < 0 &&
        ++brokerFailures >= BROKER_FAILURES_BEFORE_WIFI_RESTART) {
      Serial.printf(">>> Wi-Fi: broker unreachable %d times in a row, "
                    "restarting the Wi-Fi connection\n", brokerFailures);
      brokerFailures = 0;
      WiFi.reconnect();
    }
    return;
  }

  Serial.printf(">>> MQTT: connected in %lu ms\n", durationMs);
  connectedAtMs = millis();
  retryDelayMs = RETRY_MIN_MS;
  brokerFailures = 0;
  mqtt.publish(TOPIC_STATUS, STATUS_ONLINE, true);
  mqtt.subscribe(TOPIC_COMMANDS, 1);
}

void startWifi() {
  WiFi.mode(WIFI_STA);
  // Power save off: by default the radio sleeps between exchanges and the
  // access point holds incoming packets for it, which costs replies on a weak
  // link. The station is powered over USB, the extra current does not matter.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiDownSinceMs = millis();
}

void updateWifi() {
  static bool wasConnected = false;
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wasConnected) {
    Serial.printf(">>> Wi-Fi: connected, IP %s, signal %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (!connected && wasConnected) {
    Serial.println(">>> Wi-Fi: lost, reconnecting");
    wifiDownSinceMs = millis();
  } else if (!connected && millis() - wifiDownSinceMs >= WIFI_NUDGE_MS) {
    // Status 1: network not found (too weak a signal, or 5 GHz only);
    // 4: joining failed (often the password); 6: disconnected.
    Serial.printf(">>> Wi-Fi: still down (status %d), retrying\n",
                  WiFi.status());
    WiFi.reconnect();
    wifiDownSinceMs = millis();
  }
  wasConnected = connected;
}

}  // namespace

void networkBegin(CommandHandler onCommand) {
  commandHandler = onCommand;
  startWifi();

  // The clock synchronizes in the background as soon as the Wi-Fi is up.
  configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

  // The broker's certificate must be signed by this authority: the ESP32
  // refuses any other server.
  tlsClient.setCACert(BROKER_ROOT_CA);
  tlsClient.setTimeout(TCP_TIMEOUT_S);
  tlsClient.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_S);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(MQTT_KEEP_ALIVE_S);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt.setBufferSize(MQTT_BUFFER_BYTES);
}

void networkUpdate() {
  if (outage) {
    if ((long)(millis() - outageEndsMs) < 0) {
      state = NetworkState::WifiDown;
      return;
    }
    outage = false;
    Serial.println(">>> simulated outage over: Wi-Fi back on");
    startWifi();
  }

  updateWifi();

  if (WiFi.status() != WL_CONNECTED) {
    state = NetworkState::WifiDown;
    return;
  }

  if (mqtt.connected()) {
    state = NetworkState::Online;
    mqtt.loop();  // handles incoming commands and keep-alive
    flushPending(TOPIC_EVENTS, pendingEvents, pendingEventCount);
    flushPending(TOPIC_SAMPLES, pendingSamples, pendingSampleCount);
    return;
  }

  if (state == NetworkState::Online) {
    // state -3: the connection was closed (by the broker or the network);
    // -4: the broker stopped answering our keep-alive pings.
    Serial.printf(">>> MQTT: connection lost after %lu s (state %d), signal %d dBm\n",
                  (millis() - connectedAtMs) / 1000, mqtt.state(), WiFi.RSSI());
  }
  state = NetworkState::BrokerDown;
  if (firstAttempt || millis() - lastAttemptMs >= retryDelayMs) {
    firstAttempt = false;
    lastAttemptMs = millis();
    connectBroker();
  }
}

void networkSimulateOutage(uint32_t seconds) {
  if (outage) {
    Serial.println(">>> simulated outage already running, ignored");
    return;
  }
  // The radio goes off without a word to the broker, like a real outage: the
  // broker only notices through the keep-alive and publishes the Last Will.
  WiFi.mode(WIFI_OFF);
  outage = true;
  outageEndsMs = millis() + seconds * 1000;
  Serial.printf(">>> simulated outage: Wi-Fi off for %lu s\n",
                (unsigned long)seconds);
}

NetworkState networkState() {
  return state;
}

int networkRssi() {
  return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

time_t networkTime() {
  const time_t now = time(nullptr);
  return now >= CLOCK_SYNCED_AFTER ? now : 0;
}

bool networkPublishMeasurements(const char *json) {
  if (!mqtt.connected()) {
    return false;
  }
  // Retained: a page that opens gets the latest measurement at once.
  const bool sent = mqtt.publish(TOPIC_MEASUREMENTS, json, true);
  if (!sent) {
    Serial.println(">>> MQTT: publishing the measurements failed");
  }
  return sent;
}

void networkPublishEvent(const char *json) {
  if (mqtt.connected() && pendingEventCount == 0 &&
      mqtt.publish(TOPIC_EVENTS, json)) {
    return;
  }
  if (pendingEventCount == PENDING_EVENTS || strlen(json) >= EVENT_BYTES) {
    Serial.println(">>> MQTT: event dropped (queue full or too long)");
    return;
  }
  strcpy(pendingEvents[pendingEventCount], json);
  pendingEventCount++;
}

void networkPublishSample(const char *json) {
  if (strlen(json) >= SAMPLE_BYTES) {
    Serial.println(">>> MQTT: sample dropped (too long)");
    return;
  }
  if (mqtt.connected() && pendingSampleCount == 0 &&
      mqtt.publish(TOPIC_SAMPLES, json)) {
    return;
  }
  // Queue full: forget the oldest sample, the recent history matters more.
  if (pendingSampleCount == PENDING_SAMPLES) {
    for (int i = 1; i < PENDING_SAMPLES; i++) {
      strcpy(pendingSamples[i - 1], pendingSamples[i]);
    }
    pendingSampleCount--;
    Serial.println(">>> MQTT: oldest waiting sample dropped (queue full)");
  }
  strcpy(pendingSamples[pendingSampleCount], json);
  pendingSampleCount++;
}

bool networkPublishReply(const char *json) {
  return mqtt.connected() && mqtt.publish(TOPIC_REPLIES, json);
}
