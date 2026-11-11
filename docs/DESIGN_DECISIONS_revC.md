# Rev C.5 - design decisions (2026-10-29)

ESP32-S3-DevKitC-1 clone on two 1x22 sockets, JLCPCB Economic assembly only (PCBWay dropped). Quotes: `../QUOTES_revC.md`. Pin map: `DEVBOARD_PINMAP_revC.md`.

| # | Decision | Why |
|---|---|---|
| 1 | Dev board (third-party DevKitC-1 clone, N8R2/N16R8) on sockets | Economic PCBA refuses the ESP32 module; board bought separately, about US$6-7 |
| 2 | 100 x 100 mm board | Same $4.00 PCB price as 100x70 (cheap tier is <=100x100; 110x80 costs $8.30) |
| 3 | All four terminal blocks on the bottom edge (NMEA 2000, power, outputs, inputs), wire entry facing the edge; M3 holes in all four corners | 3D-printed case, one cable side |
| 4 | Dev board (clone, 27.94 x 57.15 mm, rows 25.40 mm apart per seller drawing) horizontal, USB flush with the right edge; top row = dev J3 pins, bottom row = dev J1 pins, pin 1 at the left | USB reachable from the case side |
| 5 | Two numbered pad rows break out the UNUSED dev-board pins plus the 3V3, 5V and GND pins (top edge = free J3 pins + SDA/SCL + GND; row below the dev board = free J1 pins + 3V3/5V/GND) plus a 3V3/5V/GND pad trio | Solder-on sensors / experiments |
| 6 | 5 V buck AP63205 -> D8 (SS14) -> dev-board 5V pin; dev board makes 3V3 | Diode stops USB back-feeding the buck. Clone's power-OR behaviour unverified |
| 7 | Isolated CAN kept (CA-IS3050G, 5 kVrms, 8.85 mm gap, copper keep-out) | Only protection the owner insisted on |
| 8 | Outputs switched on the positive (high-side AO3401A, driven by AO3400A, 22k (top) / 47k (bottom) gate divider, |Vgs| = 0.32 VIN); J3 = LGT+, GND, BUZ+, GND | Boat standard; no new part numbers |
| 9 | Hard-wired high-water bypass kept (float turns both outputs on without the ESP32) | Safety |
| 10 | LEDs (red, red) and MUTE button on the right edge, widely spaced | Prominent through a case window |
| 11 | Bold white-box knock-out labels on connectors and LEDs | Readability; only seen in a render, not on a real board |
| 12 | 100 nF caps C14663 (Basic), TVS C19077570 (Promotional), both LEDs C2286 (Basic), J2 4-pos, aux input removed | Fewer Extended lines (JLCPCB waives the fee for Basic and Promotional parts) |
| 13 | Pressure + temperature: TE MS5637-02BA03 (C3033010) at 0x76 on I2C (SDA=IO42, SCL=IO41) | BME280 and BMP581 are "Standard Only" at JLCPCB; BMP280/DPS310 not stocked. **Kept by the owner with the JLCPCB manual-review risk** (line is flagged "difficult to process", unticked by default, may be surcharged or refused) |
| 14 | Terminal pad holes 1.4 mm; terminal bodies are 12 mm deep, 45-degree wire entry facing the board edge (Degson DG127S-5.08 drawing, DORABO DB127S equivalent) | Manufacturer's recommended hole; case needs wire-entry openings on the bottom edge |
| 15 | Rev C.5 changes after the external review: buck COUT C6-C8 = 22 uF 25 V (C45783, Basic); P-FET gate pull-ups R27/R29 = 22 k so |Vgs| stays below 12 V at the TVS clamp (7.8 V at 24.4 V, 3.8 V at 12 V); LED series R5/R6 = 470 ohm (about 3 mA); second GND via on the U1 stub. PPTC F1 changed to hongjiacheng SMD1812-200-24 (C46641174, 2 A hold, 4 A trip, 24 V, Extended like the old part). All DRC silkscreen warnings cleared (silk outline inset from the edge, socket silk removed, reference designators moved). | Review findings H1, H3, L1 |
| 16 | M/Y Lada Panas logo, 32 mm circle, top silkscreen at the left (centre 17.5 / 57.3 mm). Made from `logo/lada_panas_logo.webp` by `gen/make_logo.py` (strokes thickened to 0.15 mm, JLCPCB's minimum silk line). Buck block moved 3 mm up and D8 4 mm right to make room; title/revision text moved to the top-left | The board is custom for the boat. Fine detail (small boat lettering) will print soft at this size |

## Risks / not verified
- Autorouted layout not human-reviewed; final DRC run at 0.18 mm clearance (router works to 0.20, JLCPCB minimum 0.127).
- MS5637 footprint is custom, from the datasheet package drawing (pads +-1.0 mm, 1.0 x 1.0), not independently checked; pin-1 orientation in JLCPCB's viewer still to check.
- High-side output stage and dev-board clone power/dimensions unverified; terminal blocks checked only against the equivalent Degson DG127S drawing (not DORABO's own); nothing built or tested.
- No humidity sensor now (BME280 dropped); an SHT4x could go on the SDA/SCL/3V3 pads.
- Firmware not ported (GPIO map in the pin map file); ADC2 pins (IO11/IO12) are used for IN2/IN1, fine while Wi-Fi is off.
- Terms dialog on the JLCPCB page needs the owner to accept before ordering. Nothing ordered.
