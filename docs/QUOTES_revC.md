# Rev C - JLCPCB quotes (2026-10-27, Singapore, no order placed)

Economic PCBA, top side, 2-layer 100x70 mm green, 23 BOM lines (all matched, no Standard-only flags), dev board NOT included (bought separately).

| Qty | PCB | Setup | Stencil | Parts | Extended-part fee | SMT | Hand-solder | Manual | Reflow | **Total (ex shipping)** |
|---|---|---|---|---|---|---|---|---|---|---|
| 5 | $4.00 | $8.24 | $1.55 | $31.12 | $24.72 | $1.37 | $3.61 | $4.87 | $0.91 | **$80.39** ($16.08/board) |
| 2 | $4.00 | $8.24 | $1.55 | $14.59 | $24.72 | $0.64 | $3.61 | $1.95 | $0.86 | **$60.16** ($30.08/board) |

Add one dev board per board (listing S$8.70, about US$6-7): about $13 for 2, $32 for 5, plus shipping.
Compare: Rev B Standard with ESP32 module $140.70 (5) / $109.41 (2).

## Fixed after the quote
- CPL origin: now bottom-left, Y up, same as the Gerbers (parts previously floated off the board in the viewer).
- HDR1/HDR2 rotation set to 90 in CPL (JLC's 1x22 footprint is horizontal at 0 deg); fixed in gen/post_export.py and the CPL files. The uploaded draft still had 0, so rotate them in the viewer or re-upload before any order.
- PCBWay Rev C quote not done yet; the Rev B cart item there is superseded and used the wrong CPL frame.

# Rev C - PCBWay online estimate (2026-10-27, Singapore, nothing saved or ordered)

Turnkey, top side, 5 boards, 2-layer 100x70 mm 1.6 mm green, 23 unique / 64 SMD / 0 BGA-QFP / 6 THT parts, entered by hand (no files uploaded yet).

| Item | USD |
|---|---|
| PCB (24 h) | 5.00 |
| Assembly service (17.6/board; **labour and set-up only, components NOT included**) | 88.00 |
| Member 2% off | -1.86 |
| FedEx-IP shipping to Singapore (4-7 days) | 16.64, waived by order discount |
| **Total shown** | **91.14** |

Parts cost is only quoted after PCBWay reviews an uploaded BOM. Parts at LCSC prices are about $6.3/board (about $31 for 5) plus whatever PCBWay adds, so expect roughly $125-150 for 5 boards, to be confirmed by the reviewed quote. Bigger batches drop the service fee (20 pcs 12.0/board, 50 pcs 5.9/board).

# Rev C - JLCPCB Extended-part audit (2026-10-27)
Fee $24.72 for 5 boards = 12 Extended lines x $2.06 (per unique Extended part, so independent of board count, and per-board cost falls with volume).

| Line | LCSC | Basic alternative | Verdict |
|---|---|---|---|
| 100 nF 0603 X7R 50 V | C1591 | **C14663** (Yageo CC0603KRX7R9BB104, Basic, 41M stock) | Swap, same footprint, saves $2.06 |
| Green LED 0603 | C12624 | none in 0603; Basic green is 0805 KT-0805G C2297 | Either 0805 (layout change) or red for both LEDs (no layout change), saves $2.06 |
| TVS SMBJ15CA (D1, D6) | C113989 | none (SMBJ and SMAJ all Extended) | Keep |
| PPTC 1812 1.5 A (F1) | C142805 | none | Keep |
| Header 1x22 (HDR1/2) | C41376161 | none found | Keep |
| Screw terminals x3 part numbers | C2997519/22/21 | none | Keep; could cut to fewer part numbers only by re-pinning |
| Inductor 4.7 uH (L1) | C2044594 | none found | Keep |
| AP63205 buck (U1) | C2071056 | none | Keep |
| 5 V 100 mA LDO (U3) | C42738 | AMS1117-5.0 C6187 is Basic but 15 V max input (NMEA 2000 goes to 16 V), SOT-223, 1.1 V dropout | Not recommended |
| CA-IS3050G (U4) | C528763 | none | Keep (isolated CAN is the one must-have) |

Best case: 10 Extended lines remain, saving $4.12 of $80.39 (5%). Already Basic: 10 uF 0805, red LED, SS14, both MOSFETs, all resistors, switch, P-FET.

# Rev C - Extended-fee research, round 2 (2026-10-27)
Key finding: JLCPCB Economic PCBA waives the feeder-loading fee for "Basic **and Promotional Extended**" parts (page jlcpcb.com/parts/basic_parts, 1,586 parts). Promotional parts still show the label "Extended" in the BOM table, but the fee drops.

Measured on a test BOM (5 boards): 100 nF C1591 -> **C14663** (Basic) and TVS SMBJ15CA C113989 -> **C19077570** (Promotional, same MPN/SMB package, 67k stock, $0.055):
Extended fee $24.72 -> **$18.54**, total $80.39 -> **$74.27** (-$6.12). Not yet applied to the BOM files.

Promotional parts found in the in-stock list I scraped (311 parts): TVS SMAJ/SMBJ/SMCJ/P6SMB/SMF series (C190775xx, C413760xx), ESD arrays (H5VU25UC etc.), small multilayer inductors (<=10 uH, 0603-1206). **No** promotional or Basic PTC fuse, headers, screw terminals, power inductor, buck, LDO or CAN isolator was found. The scrape missed some Basic parts (e.g. SS14 C2480), so it is not exhaustive for ICs.

Further options (each about -$2.06 fee):
1. Both LEDs red C2286 (no layout change), or 0805 green C2297.
2. J2 5-pos -> 4-pos (two terminal part numbers instead of three).
3. Drop the sockets and solder the dev board's own pins into the PCB holes (also saves $2.26 parts, loses swappability, hand-soldering).
4. Drop the on-board PPTC and rely on the external 3 A fuse (also saves $0.84 parts, loses a protection layer).
Unavoidable Extended: U1 AP63205, L1 power inductor, U3 5 V LDO (AMS1117-5.0 is Basic but only 15 V max), U4 CA-IS3050G.

# Rev C.1 - high-side outputs + Extended-fee trims (2026-10-27, JLCPCB only; no order)
Changes vs Rev C: outputs switched on the positive (AO3401A P-FET + AO3400A driver per channel), 100 nF caps C14663 (Basic), TVS C19077570 (Promotional), both LEDs red C2286, J2 4-pos (aux input dropped). PCBWay dropped (old files in quote_pcbway_STALE_not_used/).
Board: DRC 0 errors / 0 unconnected (silk warnings only), isolation gap 8.85 mm, 21 BOM lines, 76 placed parts, $6.47 parts per board at LCSC prices.
JLCPCB Economic, 5 boards, Singapore: PCB $4.00 + PCBA $67.04 (extended-part fee $15.45) = **$71.04** (was $80.39). All 21 lines matched; placement viewer shows parts on the board.
Excludes dev boards (about US$6-7 each) and shipping. Not yet quoted for 2 boards.

# Rev C.2 - 100 x 100 mm board, all I/O on one edge, breakout rows, BME280 (2026-10-28, JLCPCB only, no order)
Layout: 100 x 100 mm (same $4.00 PCB tier as 100 x 70; one dimension over 100 mm roughly doubles the PCB price: 110x80 $8.30, 120x70 $8.10, 120x84 $8.90), M3 holes in all four corners, all four terminal blocks along the bottom edge (NMEA 2000 | power | outputs | inputs), dev board horizontal with USB at the right edge, two numbered pad rows (all 44 dev-board pins) plus a 3V3/5V/GND pad trio, LEDs and MUTE button on the right edge, bold knock-out connector labels. DRC 0 errors / 0 unconnected, isolation gap 8.85 mm.

JLCPCB Economic, 5 boards, Singapore, **BME280 left off**: PCB $4.00 + PCBA $67.10 = **$71.10** (22 BOM lines; "Large Size" fee $0).
**The BME280 (C92489) is "Standard Only"**: Economic PCBA refuses it. Standard would add a $25 set-up fee and 5 mm edge rails (board becomes 110 x 100 mm, leaving the cheap PCB tier).

## Sensor candidates (JLCPCB parts library, 2026-10-28)
Economic = accepted by Economic PCBA (tested by uploading each part in a BOM + dummy CPL).
| Part | LCSC | In stock | Price (1+) | Economic? | Notes |
|---|---|---|---|---|---|
| BME280 | C92489 | 13,166 | $8.04 | **NO (Standard only)** | T/H/P; RH can stick at 100 % after condensation (Bosch forum) |
| BMP581 | C5362283 | 7,459 | $2.42 | **NO (Standard only)** | LGA-10 2x2, no stock KiCad footprint |
| BMP280 | C83291 | 0 | $12.58 pre-order | yes | not stocked |
| DPS310 | C130156 | 0 | $6.75 | yes | not stocked ("idle parts" only) |
| MS5637-02BA03 | C3033010 | 1,031 | $3.17 | **yes** | pressure + temperature, I2C only, 0x76, 3x3 mm 4-pad |
| MS5837-02BA21 | C3257869 | 1 | $15.19 | - | depth/underwater version, not needed |
| SHT45 | C5221601 | 7,446 | $5.72 | yes | T/H, +-1 % RH, condensation-tolerant heater |
| SHT40I | C3664342 | 1,031 | $1.90 | not tested | T/H, heater |
| AHT20 | C2757850 | 39,513 | $0.80 | not tested | cheap T/H, poor in condensation |
| TMP117 | C699536 | n/a | about $2.1-2.4 | yes | +-0.1 C temperature only |
| MCP9808 | C94847 | 536 | $2.27 | yes | +-0.5 C temperature only |
| DS18B20 (UMW) | C695836 MSOP-8 / C376006 TO-92 | 35k / 67k | $0.73 / $0.58 | yes | 1-wire, cheapest; probe on a cable needs no on-board chip |
Each Extended line adds $2.06 to the fee (SHT/MS5637/TMP117/MCP9808/DS18B20 are all Extended).

# Rev C.3 - MS5637 swapped in (2026-10-28): board done, quote NOT yet taken
U5 = TE MS5637-02BA03 (C3033010, $3.17, 1,031 in stock, passed the Economic test in a throw-away BOM). Pinout from the TE datasheet 06/2017 p.14: 1 VDD, 2 SDA, 3 SCL, 4 GND, I2C 0x76; custom footprint gen/fp/CUSTOM.pretty (pads +-1.0 mm, 1.0 x 1.0, from the package drawing, not independently checked). Board routed: DRC 0 errors / 0 unconnected (final DRC at 0.18 mm clearance; router works to 0.20), isolation gap 8.85 mm. Gerbers/BOM/CPL regenerated in quote_jlcpcb/ (23 BOM lines).
To do: upload quote_jlcpcb/ files to JLCPCB (Economic, 5 boards) and read the total; expected about $71 + $3.17 + $2.06 fee. Check U5 pin-1 orientation in the JLCPCB viewer. A "We've updated our Terms" dialog on the JLCPCB page needs the user to accept it.

## Rev C.3 quote draft (2026-10-28, JLCPCB Economic, 5 boards, Singapore) - no order placed
All 23 lines matched, none "Standard Only", placement viewer shows every part on the board.
- **Not including the MS5637:** PCB $4.00 + PCBA $66.69 = **$70.69**.
- **Including the MS5637 (U5 ticked by hand): PCB $4.00 + PCBA $85.68 = $89.68** (parts $46.62, Extended fee $18.54). The sensor adds about $19 for 5 boards ($15.87 parts + $3.09 fee).
- Caveat: JLCPCB marks the MS5637 line "The processing of this component is difficult": it is NOT ticked by default (quantity 0), it has to be selected manually, and after ordering it is reviewed by JLCPCB, who may charge extra for an "advanced PCBA option" or refuse it. So Economic is not guaranteed for this part. Cleaner alternatives: leave U5 unpopulated and hand-solder a part, or use an easier part (e.g. SHT45/TMP117/MCP9808 passed without such a flag in the earlier test, but they do not measure pressure).
- Draft is open in the JLCPCB tab (not added to the cart).

## Rev C.3 final draft (2026-10-28, after the pre-order review)
Final Gerbers, BOM and placement file (placement file with JLCPCB rotation offsets: SOT-23 +180, TSOT-23-6 +270, SOIC-8 +270; U5 reserved area re-routed) re-uploaded to JLCPCB: Economic, top side, 5 boards, Singapore, MS5637 ticked by hand.
**PCB $4.00 + assembly $85.78 = $89.78** (components $46.72, Extended fee $18.54, setup $8.24, stencil $1.55, SMT $1.42, hand-soldering $3.61, manual assembly $4.79, reflow $0.91). Excludes shipping and the 5 dev boards. The draft is saved by JLCPCB under Projects > Quotes; it is not in the cart and no order exists.
