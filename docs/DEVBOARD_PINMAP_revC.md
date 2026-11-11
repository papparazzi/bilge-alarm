# Rev C.3 - dev-board pin map (2026-10-28)

Source for pin names: Espressif ESP32-S3-DevKitC-1 v1.1 user guide; geometry of the OFFICIAL board from Espressif's DXF (rows 22.86 mm apart, 2.54 mm pitch, 22 pins, board 25.4 x 62.865 mm). The AliExpress board (DZQJ) is a **third-party clone, not Espressif's**: measure the first one before ordering PCBs.

On the PCB the dev board lies horizontally, USB end flush with the right edge, antenna end left, pin 1 of both rows at the left. **Top row = dev J3 pins 1-22, bottom row = dev J1 pins 1-22.** Pad rows `BR2` (above the top row, board edge) and `BR1` (below the dev board) repeat every pin, labelled `pin:name`.

## J1 row (bottom) - used pins
| Pin | GPIO | Net |
|---|---|---|
| 1, 2 | 3V3 | +3V3 (taken from the dev board for CAN isolator, I2C, pull-ups) |
| 4 | IO4 | CAN_RX |
| 5 | IO5 | CAN_TX |
| 6 | IO6 (ADC1) | VSENSE (supply voltage divider) |
| 9 | IO16 | LIGHT_CTL |
| 10 | IO17 | BUZZ_CTL |
| 12 | IO8 | BTN_MUTE |
| 15 | IO9 (ADC1) | IN4 (high-water float) |
| 16 | IO10 (ADC1) | IN3 |
| 17 | IO11 (ADC2) | IN2 |
| 18 | IO12 (ADC2) | IN1 |
| 19 | IO13 | LED_STAT |
| 20 | IO14 | LED_ALM |
| 21 | 5V | DEV_5V (buck output via SS14) |
| 22 | G | GND |
Free: 3 (RST), 7 (IO7), 8 (IO15), 11 (IO18), 13 (IO3, strapping), 14 (IO46, strapping).

## J3 row (top)
1, 21, 22 GND; **6 = IO42 = I2C SDA, 7 = IO41 = I2C SCL** (MS5637 and the pad rows); all others free: 2 TX, 3 RX, 4 IO1, 5 IO2, 8-10 IO40/39/38, 11-13 IO37/36/35 (not usable on N16R8: octal PSRAM), 14 IO0 (boot), 15 IO45 (strapping), 16 IO48, 17 IO47, 18 IO21, 19 IO20 and 20 IO19 (USB D+/D-, leave free).

## Power
AP63205 5 V buck -> SS14 -> dev-board 5V pin; the dev board's regulator makes 3V3, which also feeds U4 (CA-IS3050G VCC1), the MS5637 and the pull-ups. Espressif documents USB, 5V pin and 3V3 pin as mutually exclusive supplies: check the clone's schematic. Do not feed the 3V3 pin from outside.

## DEV BOARD GEOMETRY CHANGED (2026-10-29, Rev C.4)
The seller's dimension picture (see `07_datasheets/clone_dimensions_from_seller.png`) shows: board **27.94 x 57.15 mm**, pin span **53.34 mm** (21 x 2.54), and **25.40 mm between the two pin rows** (1.0 in), not Espressif's 22.86 mm / 62.865 mm. The PCB was re-laid out for 25.40 mm rows (Rev C.4). The end margin of the pins (1.905 mm each end) is inferred from 57.15 - 53.34, and the pin order is assumed identical to the official board (J1: 3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V, G): **check both against the first board before powering up** (pin 1 = 3V3 at the antenna end, the end opposite the two USB-C ports).

## The chosen board (listing checked 2026-10-29)
AliExpress item 1005013033169318, seller DZQJ Official Store, "ESP32-S3 Development Board DevKitC-1 N8R2 N16R8 ... 44Pin Type-C". Variants: N8R2 or N16R8, each "No Header" or "Soldered". Price S$8.73 (S$7.29 from 1 Nov sale), delivery 3-5 Nov. The listing gives **no dimensions or pinout drawing** (its "User manual (PDF)" is an empty file), so the 22.86 mm row spacing, 2.54 mm pitch and pin map are still those of Espressif's official board. The sockets on the PCB need **male pins on the underside of the dev board**: buy a "Soldered" variant, or "No Header" and solder 2 x 22 male pins yourself. The geometry above comes from the seller's picture; measure the first board with calipers (row spacing 25.40 mm, pitch 2.54 mm) if you want to confirm it. N8R2 leaves IO35-37 usable; N16R8 does not (octal PSRAM), but this design does not use them either way.

## Silkscreen photo check (2026-10-31, `07_datasheets/clone_silkscreen_photo_from_listing.png`)
Listing photo (low resolution, perspective view, antenna at top). **J3 row reads clearly and matches the map pin for pin**: GND, TX, RX, 1, 2, 42, 41, 40, 39, 38, 37, 36, 35, 0, 45, 48, 47, 21, 20. **J1 row matches from pin 4**: 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13. **Not legible: J1 pins 1-3 (3V3, 3V3, RST) and 21-22 (5V, G).** Those carry power and are the first thing to check with a meter on the real board.

## Checks to do on the first real board (before the first power-up)
1. Calipers: row spacing 25.40 mm, pitch 2.54 mm, board 27.94 x 57.15 mm, end margin of the pins about 1.9 mm.
2. Meter (unpowered): J1 pin 22 = GND (beeps to USB shell/GND pad); J3 pins 1, 21, 22 = GND; J1 pins 1 and 2 are the same net (3V3).
3. Power only the dev board by USB first (PCB not connected): J1 pin 21 = 5 V, J1 pin 1/2 = 3.3 V.
4. Check whether feeding 5 V to J1 pin 21 while USB is also plugged in is safe on this clone (Espressif's board has a diode/OR; clone unverified). The PCB's D8 only protects the buck, not the dev board.
5. Bare PCB, no dev board: apply 12 V through a current-limited supply, check +5V node, +3V3 absent (made by the dev board), CAN side isolated supply +5V_ISO, no hot parts.
