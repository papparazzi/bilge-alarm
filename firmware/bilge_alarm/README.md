# Alarm firmware (`bilge_alarm`)

The firmware that runs on the boat. Version 1.0.0-rc1. Build and flash it exactly like the bench test (see [`../README.md`](../README.md)), with `firmware/bilge_alarm` as the sketch folder. To update a board that already runs either firmware, use Upload firmware on its web page with `build/esp32.esp32.esp32s3/bilge_alarm.ino.bin`.

## Status (2026-11-10)

| Part | State |
|---|---|
| Alarm logic, counters, event log, mute | Runs on board 1. Checked through the web API with the low-voltage alarm: raised after 60 s, muted, cleared, counted, logged |
| Counters and log surviving a restart and a firmware update | Checked: both survived three firmware updates |
| Web page, settings, firmware upload over Wi-Fi | Checked on board 1 in hotspot mode |
| Inputs counted from real 12 V signals | Checked for IN1 and IN2 (2026-11-11): each counted once |
| Joining a normal Wi-Fi network without restarting | Checked on board 1 (2026-11-11) |
| Bluetooth to the Cerbo GX | Advertising starts and pauses as designed; **not checked against a Cerbo** |
| Light and buzzer with real loads, the hard-wired bypass | **Not checked yet** (boat) |
| NMEA 2000 data and Axiom alarm | Written. On the bench with no network attached it keeps running and never holds anything up, including while an alarm is being sent. **Not checked on a real network or against the Axiom** |

## What it does

- **IN1, IN2, IN3** are bilge pumps (12 V on the input = pump running). **IN4** is the high-water float.
- An input is on at 8 V or more and off below 6 V, after three readings in a row (0.15 s).
- Everything is counted **since the board was powered on**: how often each input came on, its total time on, its longest and last run, and how often each alarm was raised. The last 80 events are kept in a log.
- Counters and the log are kept through a restart, a crash and a firmware update. They return to zero only when the board loses power, or with Reset counters and log on the web page.

### Alarms

| Alarm | Raised when | Default | Buzzer |
|---|---|---|---|
| High water | The float (IN4) has been up for the delay | 2 s delay | Steady |
| Pump running too long | One pump has run without stopping for the limit | 10 minutes | Short beep every 3 s |
| Pump starting too often | One pump started this many times within an hour | Off | Off (can be switched on) |
| Supply voltage low | Supply below the limit for 60 s; clears after 10 s at 0.3 V above it | 11.5 V | Off (can be switched on) |

All limits and the buzzer choices are on the web page under Settings. Alarms clear by themselves when the cause goes away; the count and the log remember them.

### Light, buzzer, LEDs, MUTE

- **Light:** on while any alarm is active, and (by default) while any pump runs.
- **Buzzer:** as in the table, until MUTE. A new alarm sounds again.
- **Alarm LED:** solid while an alarm is active. Blinking after an alarm has cleared, until someone presses MUTE or Acknowledge on the web page: "something happened while you were away".
- **Status LED:** short flash every 2 s on the network, slow blink in setup-hotspot mode, fast blink while joining.
- **MUTE button:** a press silences and acknowledges. Holding it for 1.5 s switches light and buzzer on for as long as it is held. Holding it for 10 s resets Wi-Fi: the board forgets its network, beeps three times and opens the setup hotspot. Use that when the web page cannot be reached.
- **The hard-wired bypass is not affected by any of this.** While the float is up, the hardware holds the light and the buzzer on, and MUTE cannot silence them. That is the board's design (remove R_HW1 to change it).

### Web page

`http://bilge-alarm.local`, or `http://192.168.4.1` on the board's own hotspot. Same Wi-Fi setup as the bench test, with one change: nothing restarts the board any more. A lost network leads to the hotspot after 60 s, and an idle hotspot tries the saved network again every 5 minutes, all while the alarm keeps running.

The page shows the alarm state with a Mute button, the per-input and per-alarm counts, the event log, supply voltage (with lowest and highest since restart), pressure, temperatures, outputs, Bluetooth, the NMEA 2000 listener, Wi-Fi, settings and system information.

Reading is open to anyone on the network. Set an admin password under Settings and every change (tests, settings, Wi-Fi, firmware) asks for the login `admin`. Mute never needs a login.

JSON for other programs: `GET /api/status`, `GET /api/log`, `GET /api/settings`.

### Bluetooth for the Cerbo GX

The board advertises as a RuuviTag (format 5). The Cerbo shows it as a temperature sensor with pressure, once enabled under Settings, Integrations, Bluetooth Sensors. It carries temperature and pressure only: no alarm, no pump data, no 12 V reading. Advertising pauses while someone is connected to the setup hotspot.

### NMEA 2000

Needs the `ttlappalainen/NMEA2000` library (see the toolchain section in [`../README.md`](../README.md)). The CAN driver is our own (`can_bus.h`), so no other library is involved.

| PGN | Content | Instance | Sent |
|---|---|---|---|
| 127501 Binary Switch Bank Status | Switch 1 to 3 = pumps running, 4 = float up, 5 = any alarm | Switch bank 20 | On every change, and every 2 s |
| 127508 Battery Status | Supply voltage | Battery 5 | Every 1.5 s |
| 130310 Environmental Parameters | Barometric pressure | | Every 2 s |
| 130314 Actual Pressure | Barometric pressure, source atmospheric | 0 | Every 2 s |
| 130316 Temperature, Extended Range | Board temperature, source Inside | 0 | Every 2 s |
| 126983 Alert | One per active alarm. High water is type Alarm, the others Warning; category Technical; alert ID 1 to 8 | | Every second while active, then three times as "normal" |
| 126985 Alert Text | The alarm's text and "Bilge alarm" as location | | When raised, then every 10 s |
| 126984 Alert Response | Received: acknowledge or silence from a display acts like MUTE | | |

- Instances and the temperature source can be changed under Settings. Pick a battery instance no other device on the boat uses.
- The device announces itself as "Bilge Alarm Rev C.5", class 30 (electrical distribution), function 130 (binary event monitor), manufacturer code 2046 (the code for devices without one), load equivalency 1. It starts at address 35 and remembers the address it ends up with.
- The alert messages are built by hand, because the library has no functions for them. Their byte layout was checked field by field against the canboat PGN database (v9.2.0). Whether the Axiom accepts them is unknown until tried: it needs LightHouse 4 v4.8.164 or later.
- The Axiom does not display 127501. It should show the supply voltage as a battery, the pressure, the temperature, and the alarms.
- With NMEA 2000 output off in Settings (restart needed), the port only listens and lists the PGNs it hears.
- With the network side unpowered, the controller drops off the bus and is restarted once a second. That is expected on the bench and shows as "restarts" on the web page.

## How it is built

| File | Job |
|---|---|
| `core.h` | The alarm task: reads inputs, decides alarms, drives outputs, keeps counters and log. Runs every 50 ms at a higher priority than everything else, with a watchdog |
| `settings.h` | Owner settings in flash |
| `net.h` | Wi-Fi and the setup hotspot |
| `ble.h` | RuuviTag advertisement |
| `web.h`, `page.h` | Web server, JSON, the page |
| `board.h`, `ms5637.h` | Pressure sensor |
| `can_bus.h` | CAN driver for the NMEA 2000 port: never waits for the bus, restarts after bus-off |
| `n2k.h` | NMEA 2000 messages and alerts, on top of the NMEA2000 library |
| `config.h` | Pins and fixed constants |
| `secrets.h` | The setup hotspot password. Not in the repository: copy `secrets.example.h` and set your own |

Nothing outside `core.h` can delay the buzzer: the web server, Wi-Fi and Bluetooth only read a copy of the core's state.

## Found on the bench

- **Supply reading spikes.** Single ADC readings of the supply jump up by as much as 1 V now and then with Wi-Fi on (2785 mV at the pin, occasionally about 2990 mV). The firmware takes the median of five readings and smooths it; filtered, the reading stayed within 13.07 to 13.14 V.
- **Supply reading ceiling.** Those spikes all stopped near 2990 mV at the pin, which is 14.06 V at the terminal. If that is the converter's ceiling on this chip, a charging voltage above about 14.1 V will read as 14.1 V. To confirm with an adjustable supply.
- **ADC2 and Wi-Fi.** IN1 and IN2 had no failed reads in hotspot mode. Still to watch on a normal network.
