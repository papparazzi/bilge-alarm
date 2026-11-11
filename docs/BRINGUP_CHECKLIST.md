# First-board bring-up checklist (Rev C.5)

Nothing in this design has been built or tested yet. Do these in order and stop at the first surprise.

## 1. Measure the dev board (before it goes near the PCB)
The geometry comes from the seller's drawing, not from a measured board.
1. Calipers: row spacing 25.40 mm, pitch 2.54 mm, board 27.94 x 57.15 mm, first pin about 1.9 mm from the end.
2. Meter, unpowered: J1 pin 22 = GND (beeps to the USB shell / GND pad); J3 pins 1, 21, 22 = GND; J1 pins 1 and 2 are the same net (3V3).
3. USB only, PCB not connected: J1 pin 21 = 5 V, J1 pins 1/2 = 3.3 V.
4. Find out what the clone does with USB and the 5V pin both powered (Espressif's board has a diode OR; the clone is unverified). The PCB's D8 only protects the buck, not the dev board.

## 2. Bare PCB, no dev board
1. Current-limited bench supply, 12 V, 100 mA limit first.
2. Check the +5V node (buck output) and that +3V3 is absent (the dev board makes it).
3. Check the isolated CAN supply (+5V_ISO) and that nothing gets hot (L78L05 U3 and CA-IS3050G U4 especially at 14-16 V bus voltage).
4. Reverse the supply polarity briefly (current-limited): the P-FET Q1 must block it.

## 3. With the dev board
1. Power from 12 V only, then from USB only, then both (only if step 1.4 says it is safe).
2. Flash a test sketch: blink both LEDs, read the MUTE button, read VSENSE, read the MS5637 on I2C 0x76 (SDA IO42, SCL IO41).
3. Drive the light and buzzer outputs with a bulb and a buzzer at 9, 12 and 15 V; the P-FET gate overstress fix (|Vgs| = 0.32 VIN) should keep |Vgs| below 6 V.
4. Float input IN4 high: both outputs must switch on without the ESP32 running (hard-wired bypass).

## 4. NMEA 2000
Connect to a real bus, check the node appears, check bus voltage handling and the bus-side supply temperature at LEN 2.

## 5. Install on the boat only after the above
Fit an external 3 A fuse in the supply line. The on-board 2 A PPTC protects the board, not the wiring.
