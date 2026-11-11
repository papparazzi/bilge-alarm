# Firmware

| Folder | What it is | Status |
|---|---|---|
| `bilge_alarm/` | **The alarm firmware** for the boat: alarms, counters since power-on, event log, web page, Bluetooth for the Cerbo GX. See [`bilge_alarm/README.md`](bilge_alarm/README.md). | 1.0.0-rc1 runs on board 1. NMEA 2000 output is not written yet. |
| `bench_test/` | Bench-test firmware: exercises every part of the Rev C.5 board and shows it on a web page and the serial console. Use it to check a new board. | Ran on board 1; results in [`../docs/BENCH_RESULTS.md`](../docs/BENCH_RESULTS.md). |

The research behind the integrations (NMEA 2000, Signal K, Victron) is in [`../docs/FIRMWARE_ARCHITECTURE.md`](../docs/FIRMWARE_ARCHITECTURE.md).

## Toolchain (installed on the Windows PC, 2026-11-10)

- `arduino-cli` 1.5.1, installed with winget, at `C:\Program Files\Arduino CLI\arduino-cli.exe`
- Arduino core `esp32:esp32` 3.3.12 (includes the compiler and `esptool`)
- The bench test uses only what ships with the core
- The alarm firmware also needs the NMEA2000 library by Timo Lappalainen, 4.25.1 here. It is not in Arduino's library index, so it is installed from its GitHub repository (commands below)

To set up another PC:

```
winget install --id ArduinoSA.CLI -e
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/ttlappalainen/NMEA2000.git
arduino-cli config set library.enable_unsafe_install false
```

## Build, flash, watch

Run these from the repo root. The board settings are in `bench_test/sketch.yaml` (ESP32-S3, 8 MB flash layout, PSRAM off, console on both USB ports), so they work for both the N8R2 and N16R8 dev boards.

**Before the first build:** in each sketch folder, copy `secrets.example.h` to `secrets.h` and set your own setup-hotspot password (`AP_PASSWORD`, 8 to 63 characters). `secrets.h` is listed in `.gitignore`, so it stays on your computer. The build stops with "secrets.h: No such file" if it is missing.

```
arduino-cli compile --export-binaries firmware/bench_test
arduino-cli board list
arduino-cli upload -p COM5 firmware/bench_test
arduino-cli monitor -p COM5 -c baudrate=115200
```

Replace `COM5` with the port `board list` shows. Either USB-C port on the dev board works for flashing and for the console. If the upload cannot connect, hold BOOT, tap RST, release BOOT, and upload again.

### Arduino IDE instead of the command line

Arduino IDE 2.3.10 is installed as well and shares the ESP32 core with `arduino-cli`. Open `bench_test/bench_test.ino`; the other files appear as tabs. The IDE does not read `sketch.yaml`, so set these once under Tools:

| Tools menu | Value |
|---|---|
| Board | esp32 > ESP32S3 Dev Module |
| Port | the board's COM port |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| PSRAM | Disabled |

The serial monitor is at 115200 baud. Only one program can hold the port: close the IDE's serial monitor before flashing from the command line, and the other way round.

After the first flash you can update over Wi-Fi: open the web page, choose `firmware/bench_test/build/esp32.esp32.esp32s3/bench_test.ino.bin` under System, and press Upload firmware.

## What the bench test does

- **Web page** at `http://bilge-alarm.local` (or the IP address the console prints). It refreshes every second and shows:
  - IN1 to IN4: volts at the terminal, millivolts at the ESP32 pin, on/off, and how many ADC reads failed
  - supply voltage, barometric pressure, board temperature, CPU temperature, MUTE button
  - buttons for the light, the buzzer and both LEDs
  - the NMEA 2000 port: frames received, PGNs seen, and a self-test
  - Wi-Fi state, signal strength and address, with a form to choose a network
  - uptime, last reset reason, memory, and firmware upload
- **Setup hotspot.** With no saved network, or when the saved one cannot be joined within 20 seconds, the board opens the hotspot `BilgeAlarm-XXXX`. The password is `AP_PASSWORD` in `secrets.h` (see Build, flash, watch). Join it and the setup page opens by itself (or go to `http://192.168.4.1`). Scan, pick a network, enter its password, and press Save and connect. The board reboots and joins; if that fails, the hotspot comes back.
  - A connection lost for 60 seconds causes a reboot, which ends in the hotspot if the network is still gone.
  - A hotspot left idle for 5 minutes reboots to try the saved network again, so a router that was only switched off is picked up without anyone touching the board.
- **Serial console** at 115200 baud: a status line every 5 seconds. Keys: `s` status, `l` light, `b` buzzer, `c` CAN self-test, `i` I2C scan, `w` forget Wi-Fi, `r` reboot, `h` help.
- **LEDs** (automatic until you press an LED button on the page): the status LED blinks fast while joining, blinks slowly in hotspot mode, and gives a short flash every 2 seconds when connected. The alarm LED follows IN4.
- **MUTE button**: hold it for 1.5 seconds and the light and buzzer outputs switch on for as long as you hold it.

The web page has no login. Anyone on the same network can switch the outputs and upload firmware. That is acceptable on the bench; the real firmware needs a password on those actions.

## Bench procedure

Do `docs/BRINGUP_CHECKLIST.md` sections 1 and 2 first (meter checks on the dev board, then the bare PCB on a current-limited supply). Then:

### 1. Dev board alone, on USB

1. Flash it (above). The console should print the banner and `MS5637: no answer at 0x76`, which is expected off the PCB.
2. Join the `BilgeAlarm-XXXX` hotspot from a phone or laptop and give it your Wi-Fi details. Confirm the page then opens at `http://bilge-alarm.local`.
3. With a meter: the 5V pin (J1 pin 21) and the 3V3 pins (J1 pins 1 and 2). Write down the 5V pin value: about 4.7 V means the clone has a diode between USB and that pin, about 5.0 V means it does not.

### 2. Dev board on the PCB, 12 V only (no USB)

Set the bench supply to 12 V with a 300 mA limit. Use the web page from here on.

| Test | Do this | Expect |
|---|---|---|
| Supply reading | Compare "Supply voltage" with a meter on the 12 V terminals | Within about 0.3 V |
| Sensor | Read pressure and board temperature | About 1005 to 1015 hPa at sea level; a few degrees above room temperature; calibration check "pass"; I2C devices `0x76` |
| IN1, IN2, IN3 | Touch 12 V to each input terminal in turn | That input shows about 12 V and ON; the others stay at 0 V and off |
| Thresholds | If you have a second adjustable supply, sweep one input slowly | ON at 8 V rising, off below 6 V falling |
| ADC2 and Wi-Fi | Watch the "failed" count under IN1 and IN2 for a few minutes with the page open | **Note the numbers.** IN1 and IN2 share their converter with Wi-Fi, and this tells us how often a read is lost |
| IN4 and the bypass | Touch 12 V to IN4 with a meter (or lamp) on LGT+ and BUZ+ | IN4 ON, alarm LED on, and **both outputs at 12 V without pressing anything**: that is the hard-wired bypass |
| Bypass without the ESP32 | Power off, pull the dev board, power on, 12 V on IN4 | Both outputs still switch on |
| Outputs | Connect a 12 V bulb and the buzzer; press Light, then Buzzer | Each switches its own output only; the other stays at 0 V |
| Output at 9 V and 15 V | Repeat the output test at both supply voltages | Still switches cleanly; the FETs stay cool |
| MUTE | Press briefly, then hold | Press count goes up; holding switches both outputs on until release |
| LEDs | Press Status LED and Alarm LED, then LEDs automatic | Both light; automatic mode resumes |

Applying 12 V to an input while the board itself is unpowered does no harm: each input goes through 100 kΩ.

### 3. NMEA 2000 port

The bus side of the isolator is powered from the NMEA 2000 connector, not from the board's 12 V input.

1. Put 12 V on NET-S (+) and NET-C (-) of the NMEA 2000 terminal block.
2. Put a 120 Ω resistor across CAN-H and CAN-L (60 Ω is the proper value for a finished bus; 120 Ω is enough here).
3. Press Run self-test. Expect `PASS: frame sent and heard back`.
4. Remove the 12 V from NET-S and run it again. Expect `FAIL: no echo`. This proves the test really goes through the isolator.
5. Feel U3 (the small regulator near the NMEA 2000 connector) after a few minutes at 12 V, and again at 15 V. It should be warm at most.

On the boat, the same page lists every PGN the board hears, without transmitting anything.

### 4. Wi-Fi fallback

1. Press Forget network. The board should reboot into the hotspot.
2. Save a network with a wrong password. The hotspot should return about 20 seconds after the reboot.
3. Save the right details, then switch the router off for two minutes. The board should end up in hotspot mode, and return to the network by itself within about 5 minutes of the router coming back.

### USB and 12 V at the same time

Espressif's own board has a diode that makes this safe; this clone is unverified (step 1.3 above tells you which it is). In practice it is low risk with a powered PC: the PCB feeds the dev board through diode D8 at about 4.65 V, which is below the PC's 5 V, so the PC supplies the board and nothing flows back into it. Avoid one case: the board on 12 V with a USB cable going to a PC or hub that is switched off. The simplest rule is to flash over USB with the 12 V off, and use the web page and Wi-Fi firmware upload when the 12 V is on.
