# Bench results, Rev C.5 board 1

Dev board: ESP32-S3 clone, N16R8 (16 MB flash, 8 MB PSRAM), MAC ending 78:B4. Firmware: `firmware/bench_test` 0.1.0. Procedure: `firmware/README.md`.

## 2026-11-10

| Item | Result | How it was checked |
|---|---|---|
| Dev board pin labels against the PCB silkscreen | Pass | Photos of both, compared row by row |
| Flashing over the native USB port | Pass | First flash needed BOOT + RST to replace the factory demo; later flashes need no buttons |
| MS5637 pressure and temperature (custom footprint) | Pass | Found at 0x76, calibration check passes, 1004.6 hPa, about 30 to 33 °C, no failed reads |
| Board runs from 12 V (reverse-polarity FET, 5 V buck, D8) | Pass | Board up on the bench supply |
| Supply voltage reading | Pass | Owner's check; the board read 13.15 V |
| IN1, IN2, IN3, IN4 | Pass | Owner applied 12 V to each in turn |
| Status LED | Pass | Owner's check |
| MUTE button | Works | The board counted 6 presses; long-press lamp test not confirmed |
| Setup hotspot and web page | Pass | PC joined `BilgeAlarm-78B4`, page opened at 192.168.4.1 |
| ADC2 reads (IN1, IN2) with Wi-Fi on | 0 failed of about 94,700, then 0 of about 45,900 | **Hotspot mode with one client only.** Not yet checked with the board joined to a normal network |

To look into: the board reported its last restart as **brownout (supply dipped)**, about 10 minutes before the readings above were taken. Not yet known whether that was the moment the 12 V was switched on (harmless) or a restart while running.

## Not tested yet

- Light and buzzer outputs with a real load, at 9, 12 and 15 V
- Hard-wired bypass: FLT switching LGT+ and BUZ+, with and without the dev board fitted
- Alarm LED
- NMEA 2000 port self-test (needs 12 V on NETS / NETC and a resistor across CANH / CANL)
- Joining a normal Wi-Fi network, the fallback back to the hotspot, and ADC2 reads in that mode
- Firmware upload over Wi-Fi
- USB and 12 V connected at the same time (5V pin reading on USB alone still to be noted)
- Temperature of U3 and U4 on the NMEA 2000 side at 14 to 16 V

## 2026-11-10, later: alarm firmware 1.0.0-rc1 on board 1

Flashed over Wi-Fi from the bench-test web page (so firmware upload over Wi-Fi works). Details in `firmware/bilge_alarm/README.md`.

| Item | Result |
|---|---|
| Firmware upload over Wi-Fi | Pass, four times |
| Counters and log kept through a firmware update | Pass |
| Low-voltage alarm: raise after 60 s, mute, clear, count, log | Pass, after two fixes found by this test (a timer that fired at once, and supply-reading spikes that kept resetting it) |
| Web output test (light and buzzer pins for 5 s) | Pins switch; no load connected |
| Supply reading | Single readings spike by up to 1 V with Wi-Fi on; filtered reading steady at 13.07 to 13.14 V |
| Free memory with Wi-Fi hotspot and Bluetooth | About 100 kB, lowest 91 kB |
| NMEA 2000 output with no network attached | The controller drops off the bus and is restarted once a second; the web page still answers in about 0.25 s; no restart of the board; a test alarm was raised, sent, muted and cleared without trouble |
| NMEA 2000 alert message layout | Matches the canboat PGN database (v9.2.0) field by field; not tried on a display |

## 2026-11-11

| Item | Result |
|---|---|
| Joining a normal Wi-Fi network (no restart) | Pass: joined from the setup page, counters kept |
| ADC2 reads (IN1, IN2) on a normal network | 0 failed of about 19,000 each |
| Real 12 V on IN1 and IN2 with the alarm firmware | Each counted once |
| Bluetooth advertising with nobody on the hotspot | Running; not checked against a Cerbo |
| Wi-Fi reset by holding MUTE for 10 s | Pass: tried by the owner on board 1 |
