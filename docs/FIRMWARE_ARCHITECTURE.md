# Firmware architecture and integration research (2026-11-10)

How the real bilge-alarm firmware should talk to the Raymarine Axiom, Signal K and the Victron Cerbo GX, and what that means for the code. Web research only; nothing here has been tested on the boat. Each claim is marked:

- **[doc]** read in a primary source (vendor manual, source code, specification)
- **[report]** forum or blog report
- **[guess]** inference, to be confirmed on the bench

## Summary

1. **NMEA 2000 is the backbone.** One set of standard PGNs serves the Axiom directly, and serves Signal K with no extra code if the Cerbo's VE.Can port is on the NMEA 2000 network.
2. **The Axiom can show a third-party alarm**, but only on LightHouse 4 version 4.8.164 or later, and only through the Alert PGNs 126983 + 126985. The NMEA 2000 library has no code for these; we build them by hand.
3. **The Axiom cannot show "pump running".** It does not receive PGN 127501, and no standard PGN carries pump cycles or run time. We still send 127501 because Signal K and other displays use it.
4. **The Cerbo ignores almost everything on NMEA 2000** except tank levels. For Victron the good routes are different ones:
   - a wired "Bilge alarm" digital input for the alarm
   - a RuuviTag-format Bluetooth advertisement for temperature and pressure
5. **Venus OS 3.80 accepts Bluetooth sensor frames over Wi-Fi.** The board could send the same RuuviTag frame by HTTP and skip Bluetooth range problems entirely.
6. **Do not build on SensESP.** It needs PlatformIO and brings its own web UI. A small Signal K client of our own is enough, and optional.
7. **One hardware weakness found:** IN1 and IN2 are on ADC2, which Wi-Fi also uses. Reads can fail while Wi-Fi is on. The bench test measures how often.

## Raymarine Axiom: NMEA 2000 PGNs

Source for what the Axiom receives: the LightHouse 4 manual's PGN list **[doc]** <https://docs.raymarine.com/81406/en-US/latest/NMEA2000PGNSupport-946047B8.html>. The LightHouse 3 list could not be retrieved; treat LightHouse 3 as having no alert support **[report]**.

| Signal | PGN | Field values | Shown on Axiom (LightHouse 4) |
|---|---|---|---|
| High-water alarm | **126983** Alert + **126985** Alert Text; listen for **126984** Alert Response | Type: Alarm or Emergency Alarm. Category: Technical. Text: "Bilge high water" | Yes from v4.8.164 **[doc]**. No field report of a DIY device doing it **[report]** |
| Supply voltage | **127508** Battery Status | A battery instance no other device uses | Yes: detected automatically, can be renamed, has its own low and high voltage alarms **[doc]** |
| Barometric pressure | **130310** Environmental Parameters (also send **130314** Actual Pressure, source Atmospheric) | | Yes through 130310, which is the only source the manual names for the pressure data item **[doc]**. 130314 is received; whether it is displayed is unknown |
| Board temperature | **130316** Temperature, Extended Range (optionally 130312) | Source: Inside (2) or Main cabin (4) **[guess]** | Yes as "Inside temperature", after setting the number of interior sensors under Boat details **[doc]**. Which source value it wants is not published |
| Pumps 1 to 3 running | **127501** Binary Switch Bank Status | One bank instance, switches 1 to 3 | **No.** 127501 is not in the Axiom's receive list **[doc]** |
| High-water and mute state, for other devices | **127501** | Switches 4 and 5 of the same bank | No |
| Pump cycle count, run time | none exists | | No |

Notes:

- **Alerts need text.** The Axiom does not support alerts that rely on predefined alert codes, so 126985 must be sent with every alert **[doc]** <https://docs.raymarine.com/81406/en-US/latest/NMEA2000Alerts-33B254EE.html>. Raymarine's own example picture in that manual is a bilge alarm.
- **Alert risk.** In 2023, before v4.8.164, Digital Yacht could not get LightHouse 4 to accept its alerts **[report]** <https://panbo.com/?p=102294>. Plan a bench session on the real Axiom before relying on this.
- **Fallback alarm on the Axiom:** the low-voltage alarm of our own 127508 battery instance. It is also a real alarm worth having.
- **Showing a running pump on the Axiom** is only possible as a second, low-priority alert ("Bilge pump 1 running"). Whether that is useful or just noise is a decision for the owner. A fake tank level (127505) is possible but misleading: the Axiom has no bilge tank type **[doc]**.
- **How others do it:** Maretron RIM100 and SIM100 and the Yacht Devices run indicator all use 127501 for pump state **[doc]**. There is no standard bilge PGN.

### NMEA 2000 library

- Core library: `ttlappalainen/NMEA2000`. It has setters for 127501, 127508, 130310, 130314 and 130316, and sends the heartbeat itself. It has **no** functions for 126983, 126984 or 126985 (issue 201, still open) **[doc]**; we write those with `tN2kMsg`.
- **Decision (2026-11-10): the firmware uses its own small CAN driver (`firmware/bilge_alarm/can_bus.h`), so only the core library is needed.** The notes below are the alternatives considered.
- CAN driver: the classic `NMEA2000_esp32` driver does not support the ESP32-S3 **[doc]**. Use **`skarlsson/NMEA2000_twai`** <https://github.com/skarlsson/NMEA2000_twai>: maintained (last change June 2026), non-blocking transmit, recovers from bus-off. Hat Labs use it in their HALMET firmware, on a classic ESP32; on the S3 it is untested **[guess]**. Its source files are named `NMEA2000_esp32.*`, so never install both drivers. Install by git URL; it is not in the Arduino library manager.
- Device identity: manufacturer code 2046 (the library author's placeholder for uncertified devices), a unique number derived from the MAC address, load equivalency 1 (50 mA). Device class 30 / function 130 (Electrical Distribution / Binary Event Monitor) **[guess]**: check it against the NMEA class and function list linked from <https://ttlappalainen.github.io/NMEA2000/pg_lib_ref.html>.

## Signal K (on the Cerbo, Venus OS Large)

Signal K on the Cerbo already reads NMEA 2000 from the VE.Can port **[doc]** <https://www.victronenergy.com/live/venus-os:large>. If that port is joined to the boat's NMEA 2000 network, our PGNs arrive in Signal K with no code on our side:

| PGN | Signal K path it becomes |
|---|---|
| 127501 | `electrical.switches.bank.<instance>.<switch>.state` |
| 127508 | `electrical.batteries.<instance>.voltage` |
| 130314, source Atmospheric | `environment.outside.pressure` |
| 130316 / 130312, source Inside | `environment.inside.<instance>.temperature` |
| 126983 + 126985 | `notifications.nmea.<category>.<system>.<subsystem>.<id>`, with proper state and method. Appears only once the 126985 text has arrived |

All **[doc]**, from the `n2k-signalk` source. The VE.Can port must stay on its 250 kbit/s VE.Can / NMEA 2000 profile.

What NMEA 2000 cannot carry is pump run time, cycle counts and device health. For those, an optional **direct Signal K client** over Wi-Fi:

- Find the server by mDNS (`_signalk-ws._tcp`), with a manual host and port override in the web UI.
- Ask for access once: POST `/signalk/v1/access/requests` with a client ID, then poll the returned address until the owner approves it in the Signal K admin page. Store the token **[doc]** <https://signalk.org/specification/1.7.0/doc/access_requests.html>.
- Open a WebSocket to `/signalk/v1/stream?subscribe=none` with the token, send one meta message (units, names), then value updates. Reconnect with back-off; ask again if the token is rejected.

Paths to use:

| Signal | Path | Unit |
|---|---|---|
| High-water alarm | `notifications.flooding` (a name the specification defines) with `state: "alarm"`, `method: ["visual","sound"]` | |
| Pump running | `electrical.switches.bank.bilge.<pump>.state` (convention, not in the specification) | 1 / 0 |
| Pump run time, cycles | `electrical.pumps.bilge.<pump>.runTime`, `.cycleCount` (our own names) | s, count |
| Supply voltage | `electrical.batteries.<id>.voltage` | V |
| Pressure, temperature | `environment.inside.bilge.pressure`, `.temperature` | Pa, K |
| CPU temperature, Wi-Fi signal | `sensors.bilgeAlarm.*` (our own names) | K, dB |

Rejected options:

- **SensESP.** PlatformIO only ("Arduino IDE is not supported"), has no ESP32-S3 build target in its repository, and owns the web UI, configuration and main loop **[doc]**. It would replace our web UI, not sit beside it.
- **Plain TCP or UDP data connection.** No authentication **[doc]**.
- **Making the ESP32 a small Signal K server.** The server has no "poll another device" connection; it is not a recognised pattern for sensors **[guess]**.

One thing worth copying from the "served" idea: make the board's own `/api/status` JSON use Signal K path names and SI units. Then one piece of code produces the web UI data, the Signal K updates, and anything a future server plugin might read.

## Victron Cerbo GX

What the Cerbo actually accepts, best route first:

| Route | What it gives | Effort | Notes |
|---|---|---|---|
| **Wired digital input, type "Bilge alarm"** | A first-class alarm on the GX screen and on VRM. Type "Bilge pump" gives pump on/off, and the Cerbo counts every switch-on itself | One optocoupler or relay per signal | Inputs are 3.3 V logic with a pull-up, 5 V maximum, wanting a contact to ground **[doc]** <https://www.victronenergy.com/media/pg/Cerbo_GX/en/digital-inputs.html>. Works with Wi-Fi and Bluetooth down. The Cerbo has 4 inputs: exactly 3 pumps + 1 float, if they are free |
| **RuuviTag frame by Bluetooth** | A native temperature sensor with pressure on the GX and VRM, with no software on the Cerbo | Low | The Cerbo checks only the manufacturer ID (0x0499), format byte 5 and a 24-byte length; no hardware check **[doc]** <https://github.com/victronenergy/dbus-ble-sensors>. It carries temperature, humidity and pressure. It cannot carry an alarm or our 12 V supply (its battery field tops out at 3.6 V) |
| **The same frame by Wi-Fi** (Venus OS 3.80 "BLE bridge") | As above, without Bluetooth range limits | Low | HTTP POST to `https://<cerbo>/ble-gw/` with a token obtained in the GX pairing mode **[doc]**. Built for Shelly devices; Victron does not document it for others and may change it |
| **Garnet SeeLevel tank frame by Bluetooth** | Up to 8 "tanks" in percent plus a native 12 V field; each tank has a high-level alarm | Low | Could show pump state as 0 % or 100 % and raise an alarm through the tank alarm **[guess]**. It is a hack: the GX would call our pumps tanks. Test before relying on it |
| Node-RED virtual devices (Venus OS 3.60 and later), fed by MQTT | Cleanly named temperature, tank and pulse-counter devices on GX and VRM | Medium | No digital-input device type. One report says injected alarms do not reach VRM **[report]** |
| NMEA 2000 into VE.Can | Tank levels only | | The Cerbo does not read temperature, switch-bank or alert PGNs **[report]**. Not useful here |

Details that matter for the Bluetooth route:

- **Cerbo Bluetooth hardware.** Cerbo GX units up to serial number HQ2207 switch their internal Bluetooth off above 53 °C; those need a USB Bluetooth adapter. Later units and the Cerbo-S are fine. Some Cerbo GX MK2 variants have no Bluetooth **[doc]** <https://www.victronenergy.com/live/venus-os:start>.
- **Fixed address.** The Cerbo identifies a sensor by its Bluetooth address, so the board must keep the same one.
- **Sequence number.** The Cerbo drops a frame whose sequence number is 1 to 99 behind the last one it saw. After a reboot, the board must continue from a stored number or jump to a random one **[doc]**.
- **Enable once.** A new sensor has to be switched on under Settings, Integrations, Bluetooth Sensors.
- **Hotspot and Bluetooth do not mix.** Espressif rates Bluetooth advertising alongside a connected Wi-Fi station as stable, but alongside an active hotspot as unstable **[doc]** <https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/coexist.html>. Stop advertising while the setup hotspot is up.

The wired digital input needs a small addition to the board: an optocoupler driven from a free GPIO on the breakout pads (IO7, IO15 and IO18 are free on the J1 row), with its output side wired to the Cerbo. Nothing on the Rev C.5 PCB does this today.

## Hardware finding: IN1 and IN2 on ADC2

IN1 (IO12) and IN2 (IO11) use ADC2, which the Wi-Fi radio also uses. With Wi-Fi on, a read can fail with a timeout **[doc]** <https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc/adc_oneshot.html>. Arduino's `analogRead` hides this and returns 0, which looks exactly like "pump off" **[doc]**. The design notes said "fine while Wi-Fi is off"; the web UI now needs Wi-Fi on all the time.

- The bench test already reads the ADC through the ESP-IDF driver, takes 8 samples per reading, keeps the last good value when all 8 fail, and counts failures per input on the web page.
- If failures turn out to be rare, retrying in software is enough.
- If they are frequent, the fallback is a digital read. It has no guaranteed margin: 12 V at the terminal is 2.55 V at the pin, against a guaranteed-high level of 2.475 V on other ESP32 chips (the S3 table could not be read; assumed the same **[guess]**). It would work for 0 V against 12 V but not at a sagging battery voltage.
- A hardware fix is two bodge wires: move IN1 and IN2 to free ADC1 pins on the breakout pads (IO7 on the J1 row; IO1 and IO2 on the J3 row). Fold it into the next board revision.

## Code structure for the real firmware

Stay with `arduino-cli` and the Arduino ESP32 core 3.x. Built-in `WebServer`, `Update` and `DNSServer` are enough for the web UI; nothing in it needs an asynchronous server.

```
sensors      inputs, ADC, MS5637           -> Snapshot (SI units, timestamps, valid flags)
alarms       debounce, delay, latch, mute  -> AlarmState[] (id, severity, active, acknowledged, text)
model        the one shared store; publishers only read it
signal map   one table: signal -> JSON key, Signal K path and unit, PGN, instance, source
publishers   each can be switched on or off in the web UI:
               web JSON + page
               NMEA 2000   127501, 127508, 130310, 130314, 130316, 126983/126985 (+ 126984 in)
               Signal K    WebSocket client
               Victron     RuuviTag frame by Bluetooth or by Wi-Fi
               Cerbo input one GPIO per optocoupler
config       saved settings, edited in the web UI
net          Wi-Fi with setup hotspot, mDNS, firmware upload  (already in the bench test)
```

The alarm logic and the light and buzzer outputs must never wait for a publisher. A dead Wi-Fi network, a missing Signal K server or a bus-off NMEA 2000 port may not delay the buzzer. The hard-wired bypass on IN4 remains the last line of defence.

Settings to expose in the web UI: each publisher on or off; Wi-Fi; pump names; alarm delays and thresholds; voltage calibration; NMEA 2000 device, switch-bank and battery instances; Signal K server, token state and reset; Bluetooth on or off; a password for output control and firmware upload.

## Suggested order of work

1. Bench test of the hardware (`firmware/bench_test`, written; not yet run).
2. Alarm core + web UI + settings + NMEA 2000 basics (127501, 127508, 130310, 130314, 130316). This is already a usable alarm.
3. Axiom alert (126983, 126985, 126984), developed against the real Axiom.
4. Victron: RuuviTag frame, Bluetooth first, then the Wi-Fi bridge if range is a problem. Optocoupler outputs if the Cerbo has free digital inputs.
5. Signal K direct client, only if run time and cycle counts are wanted there.

## Owner decisions (2026-11-10)

- **The Cerbo's VE.Can port is not connected to the NMEA 2000 network.** So Signal K on the Cerbo sees none of our PGNs for now. Signal K needs either the direct Wi-Fi client (step 5) or that cable later.
- **The Cerbo is to be integrated over Bluetooth.** What that route can carry:
  - Temperature and pressure: RuuviTag frame. Native and clean.
  - Supply voltage, pump states, high-water alarm: only through the Garnet SeeLevel tank frame, where each pump or the float appears as a "tank" at 0 % or 100 % and the alarm comes from the tank's high-level alarm **[guess]**. The 12 V supply has its own field in that frame.
  - Pump cycle counts and run time: no Bluetooth format carries them.
- To test on the real Cerbo before building on it:
  - Does a tank high-level alarm from a Bluetooth tank reach the GX screen and VRM as an alarm?
  - Can one board send both frames? The Cerbo identifies a sensor by its Bluetooth address, so the two frames may need two advertising sets with different addresses **[guess]**.

## Things only the boat can answer

- LightHouse version on the Axiom (4.8.164 or later is needed for the alarm).
- Cerbo model, serial number and Venus OS version (Bluetooth works or not; 3.80 for the Wi-Fi bridge).
- Are any of the Cerbo's 4 digital inputs free?
- Which battery instances are already used on the network.
