"""
LADA PANAS BILGE MONITOR - Rev C (LOW-COST / MINIMUM-UNIQUE-PARTS) - single source of truth for the circuit.

Derived from Rev A.3 (marine-reliability revision) by trimming for cost and sourcing.  Goals:
  * commercial-grade parts only (no -Q1 / AEC-Q parts), the commonest MPNs on the market
  * as few unique MPNs as possible, even if a part is larger or dearer than strictly needed
  * isolated CAN is kept (protects the NMEA 2000 backbone and everything on it)
  * everything else relaxed (see docs/DESIGN_DECISIONS_revC.md)

Run:  python3 design_revC.py          -> checks the netlist and writes ../mfg/BOM_revC.csv, netlist_revC.csv, cost.
Data below: LCSC code / unit price @1 / stock are as read from the LCSC web API on 2026-10-26 (a sourcing proxy only -
PCBWay buys from its own channels, see the decision log).
"""
import csv
import os
import re
import collections

REV = "B"
DATE = "2026-10-26"

# MPN -> (manufacturer, description, package/footprint, LCSC code, price@1 USD, LCSC stock 2026-10-26)
MPN = {
    "HC-PM254-8.5H-1x22PZ": ("Hong Cheng (any 1x22 2.54 mm female socket)", "1x22 female header 2.54 mm, 8.5 mm tall (dev-board socket)", "THT 2.54 mm 1x22", "C41376161", 0.2278, 16560),
    "CA-IS3050G":          ("Chipanalog", "5 kVrms isolated CAN transceiver, VCC1 2.5-5.5 V, VCC2 4.5-5.5 V", "SOIC-8 wide (7.5 mm body)", "C528763", 1.1908, 5869),
    "AP63205WU-7":         ("Diodes Inc", "5 V 2 A buck, 3.8-32 V in, TSOT-26", "TSOT-23-6", "C2071056", 0.4456, 42142),
    "L78L05ABUTR":         ("STMicroelectronics", "5 V 100 mA linear regulator, 30 V in", "SOT-89-3", "C42738", 0.0751, 105485),
    "AO3400A":             ("Alpha & Omega", "N-MOSFET 30 V 5.7 A, VGS +-12 V, logic level", "SOT-23", "C20917", 0.0897, 662195),
    "AO3401A":             ("Alpha & Omega", "P-MOSFET -30 V -4 A, VGS +-12 V", "SOT-23", "C15127", 0.0967, 218005),
    "SMBJ15CA":            ("hongjiacheng (JLCPCB promotional part)", "TVS 600 W bidirectional, VRWM 15 V, VBR 16.7-18.5 V, VC 24.4 V", "DO-214AA (SMB)", "C19077570", 0.0549, 67145),
    "MS563702BA03-50":     ("TE Connectivity", "Barometric pressure + temperature sensor, I2C 0x76, 300-1200 mbar", "QFN-4 (3x3)", "C3033010", 3.1738, 1031),
    "SS14":                ("MDD (any)", "Schottky 40 V 1 A", "DO-214AC (SMA)", "C2480", 0.0193, 1529300),
    "KT-0603R":            ("Hubei KENTO (any)", "LED red 0603", "0603", "C2286", 0.0077, 5245500),
    "SMD1812-200-24":      ("hongjiacheng", "PPTC 2 A hold, 4 A trip, 24 V", "1812", "C46641174", 0.1410, 16534),
    "1812L150/24MR":       ("Littelfuse", "PPTC 1.5 A hold, 3 A trip, 24 V", "1812", "C142805", 0.094, 17410),
    "SRN6045TA-4R7M":     ("Bourns", "4.7 uH 20 %, shielded, 6x6 mm (4.7 uH = AP63205 datasheet Table 3 for 5 V out)", "SMD 6x6 mm", "C2044594", 0.3323, 570),
    "TS-1187A-B-A-B":      ("XKB (any)", "Tactile switch 5.1x5.1 mm SMD", "SMD 5.1x5.1", "C318884", 0.0208, 921740),
    "DB127S-5.08-2P-GN-S": ("DORABO", "Screw terminal 5.08 mm, 2 pos, side entry", "THT 5.08 mm", "C2997519", 0.3115, 1950),
    "DB127S-5.08-4P-GN-S": ("DORABO", "Screw terminal 5.08 mm, 4 pos, side entry", "THT 5.08 mm", "C2997521", 0.7014, 791),
    # passives - any brand, same value / package / voltage
    "0603WAF1001T5E":      ("UNI-ROYAL (any)", "1 k 1 % 0603", "0603", "C21190", 0.0017, 8337700),
    "0603WAF5101T5E":      ("UNI-ROYAL (any)", "5.1 k 1 % 0603", "0603", "C23186", 0.0016, 16972000),
    "0603WAF1002T5E":      ("UNI-ROYAL (any)", "10 k 1 % 0603", "0603", "C25804", 0.0022, 261800),
    "0603WAF2702T5E":      ("UNI-ROYAL (any)", "27 k 1 % 0603", "0603", "C22967", 0.0047, 400300),
    "0603WAF4702T5E":      ("UNI-ROYAL (any)", "47 k 1 % 0603", "0603", "C25819", 0.0036, 754900),
    "0603WAF1003T5E":      ("UNI-ROYAL (any)", "100 k 1 % 0603", "0603", "C25803", 0.0031, 9066300),
    "CC0603KRX7R9BB104":   ("Yageo", "100 nF 50 V X7R 0603 (JLCPCB Basic)", "0603", "C14663", 0.0129, 41553667),
    "CL21A106KAYNNNE":     ("Samsung (any)", "10 uF 25 V X5R 0805", "0805", "C15850", 0.0441, 2232260),
    "CL21A226MAQNNNE":     ("Samsung", "22 uF 25 V X5R 0805 (JLCPCB Basic; buck COUT per AP63205 DS41326)", "0805", "C45783", 0.0600, 3984410),
    "0603WAF4700T5E":      ("UNI-ROYAL (any)", "470 ohm 1 % 0603", "0603", "C23179", 0.0017, 5000000),
    "0603WAF2202T5E":      ("UNI-ROYAL (any)", "22 k 1 % 0603", "0603", "C31850", 0.0017, 2000000),
}

FP = dict(R="Resistor_SMD:R_0603_1608Metric", C6="Capacitor_SMD:C_0603_1608Metric", C8="Capacitor_SMD:C_0805_2012Metric",
          LED="LED_SMD:LED_0603_1608Metric", SMB="Diode_SMD:D_SMB", SMA="Diode_SMD:D_SMA", SOT23="Package_TO_SOT_SMD:SOT-23",
          SOT89="Package_TO_SOT_SMD:SOT-89-3", TSOT6="Package_TO_SOT_SMD:TSOT-23-6", SO8W="Package_SO:SOIC-8_7.5x5.85mm_P1.27mm",
          SOCK="Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical", PAD22="Connector_PinHeader_2.54mm:PinHeader_1x22_P2.54mm_Vertical",
          PAD3="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical", LGA8="Package_LGA:Bosch_LGA-8_2.5x2.5mm_P0.65mm_ClockwisePinNumbering", MS5637="CUSTOM:MS5637-02BA03_QFN-4_3x3mm", PPTC="Fuse:Fuse_1812_4532Metric", IND="Inductor_SMD:L_Bourns_SRN6045TA",
          SW="Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", T2="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
          T4="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-4-5.08_1x04_P5.08mm_Horizontal",
          T5="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-5-5.08_1x05_P5.08mm_Horizontal", HOLE="MountingHole:MountingHole_3.2mm_M3")

parts = []


def P(ref, mpn, fp, pins, note="", block=""):
    parts.append(dict(ref=ref, mpn=mpn, fp=FP[fp], pins=pins, note=note, block=block))


_n = {"R": 0, "C": 0}
RV = {"1k": "0603WAF1001T5E", "5.1k": "0603WAF5101T5E", "10k": "0603WAF1002T5E", "27k": "0603WAF2702T5E",
      "47k": "0603WAF4702T5E", "100k": "0603WAF1003T5E", "470": "0603WAF4700T5E", "22k": "0603WAF2202T5E"}


def R(val, a, b, note="", block=""):
    _n["R"] += 1
    P(f"R{_n['R']}", RV[val], "R", {"1": a, "2": b}, note or val, block)


def C(val, a, b, note="", block=""):
    _n["C"] += 1
    if val == "100n":
        P(f"C{_n['C']}", "CC0603KRX7R9BB104", "C6", {"1": a, "2": b}, note or "100 nF", block)
    elif val == "22u":
        P(f"C{_n['C']}", "CL21A226MAQNNNE", "C8", {"1": a, "2": b}, note or "22 uF", block)
    else:
        P(f"C{_n['C']}", "CL21A106KAYNNNE", "C8", {"1": a, "2": b}, note or "10 uF", block)


# ================================================================ POWER ENTRY
B = "Power"
P("J1", "DB127S-5.08-2P-GN-S", "T2", {"1": "VBAT_IN", "2": "GND"}, "12 V IN (external 3 A fuse required)", B)
P("D1", "SMBJ15CA", "SMB", {"1": "VBAT_IN", "2": "GND"}, "supply TVS, bidirectional", B)
P("F1", "SMD1812-200-24", "PPTC", {"1": "VBAT_IN", "2": "VFUSED"}, "board PPTC 2 A hold, 4 A trip (1.5 A hold derated too far at 50 C behind the MFD)", B)
# reverse polarity: P-FET, drain = input side, source = protected rail; gate divider keeps |VGS| = 0.32 x VIN
P("Q1", "AO3401A", "SOT23", {"1": "Q1_G", "2": "VIN", "3": "VFUSED"}, "reverse-polarity P-FET (pin 1 G, 2 S, 3 D)", B)
R("47k", "VIN", "Q1_G", "Q1 gate divider top", B)
R("100k", "Q1_G", "GND", "Q1 gate divider bottom", B)
for _ in range(3):
    C("10u", "VIN", "GND", "buck CIN", B)
C("100n", "VIN", "GND", "HF at VIN pin", B)

# ================================================================ 5 V BUCK (feeds the dev board 5V pin; the dev board makes the 3.3 V)
B = "Buck"
P("U1", "AP63205WU-7", "TSOT6", {"1": "+5V", "2": "EN_BUCK", "3": "VIN", "4": "GND", "5": "SW", "6": "BST"},
  "5 V buck; pin 1 = FB tied to output (AP63205 datasheet Fig. 21)", B)
R("100k", "VIN", "EN_BUCK", "EN divider top", B)
R("47k", "EN_BUCK", "GND", "EN divider bottom (EN = 0.32 VIN)", B)
C("100n", "BST", "SW", "bootstrap", B)
P("L1", "SRN6045TA-4R7M", "IND", {"1": "SW", "2": "+5V"}, "4.7 uH: confirmed against AP63205 datasheet DS41326 Table 3 (5 V out)", B)
for _ in range(3):
    C("22u", "+5V", "GND", "buck COUT (22 uF per AP63205 datasheet)", B)
# series Schottky: our 5 V can feed the dev board but USB 5 V can never back-feed the buck
P("D8", "SS14", "SMA", {"1": "DEV_5V", "2": "+5V"}, "+5V -> dev-board 5V pin (cathode = DEV_5V); blocks back-feed from the dev-board USB port", B)

# ================================================================ DEV BOARD (ESP32-S3-DevKitC-1 pattern, 2 x 22 pins; clone: rows 25.40 mm apart, see DEVBOARD_PINMAP_revC.md)
B = "MCU"
# J1 column (left, pin 1 nearest the antenna end) and J3 column (right), pin numbers as in Espressif's DevKitC-1 v1.1 user guide
J1NAMES = {1: "3V3", 2: "3V3", 3: "RST", 4: "IO4", 5: "IO5", 6: "IO6", 7: "IO7", 8: "IO15", 9: "IO16", 10: "IO17", 11: "IO18", 12: "IO8", 13: "IO3", 14: "IO46",
           15: "IO9", 16: "IO10", 17: "IO11", 18: "IO12", 19: "IO13", 20: "IO14", 21: "5V", 22: "GND"}
J3NAMES = {1: "GND", 2: "TX", 3: "RX", 4: "IO1", 5: "IO2", 6: "IO42", 7: "IO41", 8: "IO40", 9: "IO39", 10: "IO38", 11: "IO37", 12: "IO36", 13: "IO35", 14: "IO0",
           15: "IO45", 16: "IO48", 17: "IO47", 18: "IO21", 19: "IO20", 20: "IO19", 21: "GND", 22: "GND"}
_u1 = {1: "+3V3", 2: "+3V3", 4: "CAN_RX", 5: "CAN_TX", 6: "VSENSE_A", 9: "LIGHT_CTL", 10: "BUZZ_CTL", 12: "BTN_MUTE", 15: "IN4_A", 16: "IN3_A", 17: "IN2_A",
       18: "IN1_A", 19: "LED_STAT", 20: "LED_ALM", 21: "DEV_5V", 22: "GND"}
_u3 = {1: "GND", 6: "I2C_SDA", 7: "I2C_SCL", 21: "GND", 22: "GND"}
hdr1 = {str(k): _u1.get(k, f"J1_P{k}") for k in range(1, 23)}      # unused pins get their own net so the breakout pad row can reach them
hdr2 = {str(k): _u3.get(k, f"J3_P{k}") for k in range(1, 23)}
P("HDR1", "HC-PM254-8.5H-1x22PZ", "SOCK", hdr1, "dev board J1: 1,2 3V3 | 3 RST | 4-7 GPIO4-7 | 8 GPIO15 | 9 GPIO16 | 10 GPIO17 | 11 GPIO18 | 12 GPIO8 | 13 GPIO3 | 14 GPIO46 | 15 GPIO9 | 16 GPIO10 | 17 GPIO11 | 18 GPIO12 | 19 GPIO13 | 20 GPIO14 | 21 5V | 22 G", B)
P("BR1", None, "PAD22", {k: v for k, v in hdr1.items() if int(k) not in _u1 or int(k) in (1, 2, 21, 22)}, "breakout pads for the UNUSED dev-board J1 pins + its 3V3/5V/GND pins", B)
P("BR2", None, "PAD22", {k: v for k, v in hdr2.items() if int(k) not in _u3 or int(k) in (1, 6, 7, 21, 22)}, "breakout pads for the unused dev-board J3 pins + GND + I2C SDA/SCL", B)
P("PW1", None, "PAD3", {"1": "+3V3", "2": "DEV_5V", "3": "GND"}, "power pads for a sensor: 3V3, 5V, GND", B)
P("HDR2", "HC-PM254-8.5H-1x22PZ", "SOCK", hdr2, "dev board J3: 1 G | 2 TX | 3 RX | 4 GPIO1 | 5 GPIO2 | 6-13 GPIO42,41,40,39,38,37,36,35 | 14 GPIO0 | 15 GPIO45 | 16 GPIO48 | 17 GPIO47 | 18 GPIO21 | 19 GPIO20 | 20 GPIO19 | 21,22 G", B)
C("100n", "+3V3", "GND", "3V3 decoupling for U4 side", B)
R("470", "LED_STAT", "LED_STAT_A", "LED series (about 3 mA)", B)
P("D2", "KT-0603R", "LED", {"1": "GND", "2": "LED_STAT_A"}, "status LED red (cathode pin 1); same part as D3 so it is JLCPCB Basic", B)
R("470", "LED_ALM", "LED_ALM_A", "LED series (about 3 mA)", B)
P("D3", "KT-0603R", "LED", {"1": "GND", "2": "LED_ALM_A"}, "alarm LED red (cathode pin 1)", B)
R("10k", "+3V3", "BTN_MUTE", "MUTE pull-up", B)
P("SW3", "TS-1187A-B-A-B", "SW", {"1": "BTN_MUTE", "2": "GND"}, "MUTE (long press = lamp/buzzer test); RESET and BOOT are the dev board's own buttons", B)

# ================================================================ INPUTS (non-isolated, ADC thresholds)
B = "Inputs"
P("J2", "DB127S-5.08-4P-GN-S", "T4", {str(k): f"IN{k}_RAW" for k in range(1, 5)}, "1-3 pump run, 4 high-water float (8-16 V = active); the 5th (aux) input was dropped in this revision, its divider R16/R17/C14 is a spare", B)
for k in range(1, 5):
    R("100k", f"IN{k}_RAW", f"IN{k}_A", f"IN{k} divider top", B)
    R("27k", f"IN{k}_A", "GND", f"IN{k} divider bottom (12 V -> 2.55 V)", B)
    C("100n", f"IN{k}_A", "GND", f"IN{k} filter", B)
R("100k", "VIN", "VSENSE_A", "VSENSE divider top", B)
R("27k", "VSENSE_A", "GND", "VSENSE divider bottom", B)
C("100n", "VSENSE_A", "GND", "VSENSE filter", B)

# ================================================================ OUTPUTS: low-side N-FETs, firmware + hard-wired high-water bypass
B = "Outputs"
P("J3", "DB127S-5.08-4P-GN-S", "T4", {"1": "LIGHT_OUT", "2": "GND", "3": "BUZZ_OUT", "4": "GND"}, "1 LIGHT+ (switched +12 V), 2 GND, 3 BUZZER+ (switched +12 V), 4 GND", B)
# high-side: P-FET (Q6/Q7, S = VIN, D = load +) turned on by a logic N-FET (Q2/Q3) pulling the P-FET gate divider down.
# The 22 k (top, VIN-to-gate) / 47 k (bottom) divider (R27-R30) limits |VGS| to 0.32 VIN (7.8 V at the 24.4 V TVS clamp; AO3401A max 12 V), still -3.8 V at 12 V.
for ch, ctl, out in (("L", "LIGHT_CTL", "LIGHT_OUT"), ("B", "BUZZ_CTL", "BUZZ_OUT")):
    q = "Q2" if ch == "L" else "Q3"
    P(q, "AO3400A", "SOT23", {"1": f"{ch}_G", "2": "GND", "3": f"{ch}_PD"}, f"{'light' if ch == 'L' else 'buzzer'} gate-driver N-FET (pin 1 G, 2 S, 3 D)", B)
    R("1k", ctl, f"{ch}_G", "gate series", B)
    R("100k", f"{ch}_G", "GND", "gate pull-down (OFF in reset)", B)
    P("D4" if ch == "L" else "D5", "SS14", "SMA", {"1": out, "2": "GND"}, "flyback across the load (cathode = switched +12 V, anode = GND)", B)
# hard-wired high-water bypass: the float (IN4) turns both outputs on without the ESP32 (DNP R_hw1 to disable)
R("100k", "IN4_RAW", "HW_G", "R_HW1 - DNP to disable the hardware bypass", B)
R("47k", "HW_G", "GND", "R_HW2 - HW gate divider (12 V -> 3.8 V; 29 V surge -> 9.3 V < 12 V VGS max)", B)
P("Q4", "AO3400A", "SOT23", {"1": "HW_G", "2": "GND", "3": "L_PD"}, "hardware high-water -> light (wired-OR with Q2)", B)
P("Q5", "AO3400A", "SOT23", {"1": "HW_G", "2": "GND", "3": "B_PD"}, "hardware high-water -> buzzer (wired-OR with Q3)", B)

# ================================================================ ISOLATED NMEA 2000
B = "NMEA2000"
P("J4", "DB127S-5.08-4P-GN-S", "T4", {"1": "NET_S", "2": "NET_C", "3": "CANH_J", "4": "CANL_J"}, "NMEA 2000: 1 NET-S (+12 V), 2 NET-C (V-), 3 CAN-H, 4 CAN-L; shield not connected on board", B)
P("D6", "SMBJ15CA", "SMB", {"1": "NET_S", "2": "NET_C"}, "bus-side TVS", B)
P("D7", "SS14", "SMA", {"1": "NET_SP", "2": "NET_S"}, "NET-S reverse protection (KiCad D_SMA: pin 1 = cathode = NET_SP, pin 2 = anode = NET_S)", B)
C("10u", "NET_SP", "NET_C", "LDO input", B)
P("U3", "L78L05ABUTR", "SOT89", {"1": "+5V_ISO", "2": "NET_C", "3": "NET_SP"}, "bus-side 5 V (pin 1 OUT, 2 GND, 3 IN)", B)
C("10u", "+5V_ISO", "NET_C", "LDO output", B)
C("100n", "+5V_ISO", "NET_C", "VCC2 bypass at U4", B)
P("U4", "CA-IS3050G", "SO8W", {"1": "+3V3", "2": "CAN_RX", "3": "CAN_TX", "4": "GND", "5": "NET_C", "6": "CANL_J", "7": "CANH_J", "8": "+5V_ISO"},
  "NOTE pin order differs from ISO1042: pin 2 = RXD, pin 3 = TXD", B)
C("100n", "+3V3", "GND", "VCC1 bypass at U4", B)
R("10k", "+3V3", "CAN_TX", "TXD pull-up: recessive during reset/boot", B)

# ================================================================ MS5637 barometric pressure + temperature (I2C, address 0x76; TE datasheet 06/2017 p.14)
B = "Sensor"
P("U5", "MS563702BA03-50", "MS5637", {"1": "+3V3", "2": "I2C_SDA", "3": "I2C_SCL", "4": "GND"}, "pin 1 VDD, 2 SDA, 3 SCL, 4 GND (pads seen from above: pin 1 top-left, 2 bottom-left, 3 bottom-right, 4 top-right)", B)
R("5.1k", "+3V3", "I2C_SDA", "I2C pull-up", B)
R("5.1k", "+3V3", "I2C_SCL", "I2C pull-up", B)
C("100n", "+3V3", "GND", "MS5637 VDD bypass (datasheet: 100 nF at the VDD pin)", B)

# high-side load switches (appended last so earlier reference numbers are unchanged)
B = "Outputs"
for ch, out, q in (("L", "LIGHT_OUT", "Q6"), ("B", "BUZZ_OUT", "Q7")):
    P(q, "AO3401A", "SOT23", {"1": f"{ch}_PG", "2": "VIN", "3": out}, f"{'light' if ch == 'L' else 'buzzer'} high-side P-FET (pin 1 G, 2 S, 3 D)", B)
    R("22k", "VIN", f"{ch}_PG", "P-FET gate pull-up 22 k (off by default); |VGS| = VIN x 22/(22+47) = 0.32 VIN when driven", B)
    R("47k", f"{ch}_PG", f"{ch}_PD", "P-FET gate divider bottom 47 k", B)

# mounting
for i in range(1, 5):
    P(f"H{i}", None, "HOLE", {}, "M3 mounting hole (position TBD from enclosure)", "Mech")


def nat(r):
    m = re.match(r"([A-Z]+)(\d+)", r)
    return (m.group(1), int(m.group(2)))


def nets():
    n = collections.defaultdict(list)
    for p in parts:
        for pin, net in p["pins"].items():
            n[net].append((p["ref"], pin))
    return n


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "mfg")
    bad = [(k, v) for k, v in nets().items() if len(v) < 2]
    print("SINGLE-PIN NETS (must be only intentional):", bad)
    placed = [p for p in parts if p["mpn"]]
    by = collections.OrderedDict()
    for p in placed:
        by.setdefault(p["mpn"], []).append(p)
    # BOM
    tot = 0.0
    rows = []
    for i, (mpn, ps) in enumerate(sorted(by.items(), key=lambda kv: nat(kv[1][0]["ref"])), 1):
        man, desc, pkg, lcsc, price, stock = MPN[mpn]
        refs = sorted((p["ref"] for p in ps), key=nat)
        q = len(ps)
        tot += q * price
        rows.append([i, ",".join(refs), q, man, mpn, desc, ps[0]["fp"].split(":")[1], "THT" if "DB127" in mpn else "SMD",
                     "Equivalent OK: same value/package/rating, commercial grade" if mpn[:2] in ("06", "CL") else
                     ("Exact MPN preferred; equivalent with same pinout/ratings OK after confirmation" ), lcsc, price, stock])
    with open(os.path.join(out, "BOM_revC.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Item", "Designator", "Qty", "Manufacturer", "Mfg Part #", "Description", "Footprint", "Mounting",
                    "Substitution", "LCSC", "Unit USD @1 (LCSC)", "LCSC stock 2026-10-26"])
        w.writerows(rows)
    with open(os.path.join(out, "netlist_revC.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Net", "Pins"])
        for k, v in sorted(nets().items()):
            w.writerow([k, " ".join(f"{r}.{p}" for r, p in v)])
    n_thr = sum(1 for p in placed if "DB127" in p["mpn"] or p["mpn"] == "TYPE-C-31-M-12")
    print(f"{len(placed)} placed parts, {len(by)} unique MPNs, {n_thr} THT/pegged parts, {len(nets())} nets")
    print(f"Component cost per board at LCSC qty-1 prices: ${tot:.2f}")
    low = [(m, MPN[m][5]) for m in by if MPN[m][5] < 3000]
    print("LCSC stock < 3000:", low)


if __name__ == "__main__":
    main()
