# routine-station

[Version française](README.fr.md)

A connected monitoring station: an ESP32 reads sensors, drives a stepper motor
and streams its measurements to a cloud MQTT broker, while a web page shows them
live and sends commands back. When a flame is detected, the alarm and the motor
stop are handled on the board in real time, with or without a network.

**Status:** step 2 of 6 done (real-time flame alarm, measured). See the
[roadmap](#roadmap).

## Demo

_The live page arrives at step 4, the demo GIF at step 6._

## Architecture

Target architecture, built step by step:

```mermaid
flowchart LR
    subgraph station["Station"]
        sensors["Sensors<br/>temperature, light, flame"]
        esp["ESP32 firmware<br/>Arduino + FreeRTOS"]
        actuators["Actuators<br/>stepper motor, LED, buzzer"]
        sensors --> esp --> actuators
    end

    broker[("HiveMQ Cloud<br/>MQTT broker")]

    subgraph site["Website on Vercel"]
        page["/routine page"]
        api["API route<br/>login + validation"]
    end

    esp -- "measurements, MQTT over TLS" --> broker
    broker -- "commands" --> esp
    broker -- "live measurements, read-only access" --> page
    page -- "command request" --> api
    api -- "validated command, command access" --> broker
```

The flame alarm lives entirely inside the station box: it never waits for the
broker or the Wi-Fi.

## Hardware

Everything comes from an Arduino UNO starter kit plus an ESP32 board.

| Part | Role |
|------|------|
| ESP32 DevKit V1 (ESP32-WROOM-32, 30 pins) | Microcontroller, Wi-Fi |
| LM35 | Temperature |
| Photoresistor + 10 kΩ resistor | Ambient light |
| Infrared flame sensor + 10 kΩ resistor | Flame detection |
| 28BYJ-48 stepper motor + ULN2003 driver | Motor |
| LED, buzzer | Alarm |

Pin-by-pin wiring: [docs/wiring.md](docs/wiring.md) (in French).

## How it works

_Filled in as the steps land._ The plan:

- The firmware samples the sensors, drives the motor and publishes measurements
  over MQTT.
- The `/routine` page subscribes to those measurements and displays them live.
- Commands (start or stop the motor, trigger or silence the alarm, set
  thresholds) go from the page through a protected API route to the broker, then
  to the ESP32.

The MQTT topics and JSON payloads are specified in
[docs/protocol.md](docs/protocol.md) (in French, filled in at step 3). It is the
shared reference with the website repository.

## Technical choices

One short note per important decision, in [docs/decisions/](docs/decisions/) (in
French):

- [0001: PlatformIO and the Arduino framework](docs/decisions/0001-platformio-arduino.md)
- [0002: a home-made stepper motor driver](docs/decisions/0002-pilote-moteur-maison.md)
- [0003: the flame alarm in a FreeRTOS task](docs/decisions/0003-alarme-tache-freertos.md)
- [0004: telling a flame from daylight](docs/decisions/0004-flamme-ou-lumiere-du-jour.md)

## Real-time guarantees

The flame alarm runs in its own FreeRTOS task, with a higher priority than
`loop()`. It samples the sensors every 2 ms and, on a flame, turns on the LED
and the buzzer and locks the motor without going through `loop()`. A watchdog
reboots the board if the task ever stops. Measured on the board:

| Measure | Result |
|---------|--------|
| Sampling period | 2.000 ms on average, always within 1.69–2.26 ms, even while `loop()` is stalled |
| Flame onset to alarm | 88 ms; 72 ms with `loop()` stalled 500 ms per pass |
| Same check done in a stalled `loop()` | noticed only after 127 ms in the test, up to 500 ms |
| False alarms | none on shadows and hands |

![Raw sensor captures: shadow, hand, lighter, lighter with loop() stalled](docs/assets/02-alarme-flamme.png)

The bare infrared sensor also sees daylight, so the photoresistor serves as a
reference to tell a flame from a change of daylight
([decision 0004](docs/decisions/0004-flamme-ou-lumiere-du-jour.md)). Thresholds
were chosen by recording raw signals and replaying them through the algorithm.
Known limits: about 20 cm of range in daylight, see the
[step 2 journal](docs/journal/02-alarme-flamme.md) (in French).

## Security model

_Implemented at steps 3 and 5._ The design:

- The broker has two sets of credentials: a read-only one used by the public
  page, and a command one used only by a server-side API route.
- The API route checks that the owner is logged in (Auth.js, GitHub login
  restricted to one account) and validates every command: allowed type, values
  within bounds.
- The ESP32 validates every command again and talks to the broker over TLS.
- No secret is committed: the real configuration file is git-ignored and an
  example file is committed in its place.

## Roadmap

- [x] **Step 0, setup**: repository structure, PlatformIO, README skeleton,
  conventions (`v0.0-setup`)
- [x] **Step 1, local sensors and motor**: everything works, results on the
  serial monitor (`v0.1-sensors`)
- [x] **Step 2, real-time flame alarm**: dedicated high-priority task, measured
  reaction time (`v0.2-alarm`)
- [ ] **Step 3, cloud broker**: Wi-Fi, MQTT over TLS, automatic reconnection,
  measurements out, commands in (`v0.3-mqtt`)
- [ ] **Step 4, live `/routine` page**: in the website repository
  (`v0.4-live-page`)
- [ ] **Step 5, commands from the web**: API route protected by login
  (`v0.5-commands`)
- [ ] **Step 6, polish**: complete README, wiring diagram, demo GIF, wrap-up
  (`v1.0`)

The story of each step is in [docs/journal/](docs/journal/) (in French).

## Build and flash

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```sh
pio run                # build
pio run -t upload      # flash the board over USB
pio device monitor     # serial monitor, 115200 baud
```

## Repository layout

```
platformio.ini     board and toolchain configuration
include/pins.h     every GPIO in one place
src/               firmware, one module per responsibility
docs/protocol.md   MQTT topics and JSON payloads
docs/wiring.md     pin-by-pin wiring
docs/journal/      one short entry per step
docs/decisions/    one short note per important choice
```

## What I learned

_Written at the end of the project._

## License

[MIT](LICENSE)
