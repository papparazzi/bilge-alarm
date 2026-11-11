// Bilge alarm Rev C.5 - bench test firmware.
// Exercises every part of the board and shows it on a web page and the serial console:
// IN1-IN4 and supply voltage, MS5637, MUTE button, both LEDs, light and buzzer outputs,
// the isolated NMEA 2000 port, and Wi-Fi with a setup hotspot.
// This is NOT the alarm firmware: nothing here raises an alarm on its own.
// (The hard-wired high-water bypass on IN4 works regardless of any firmware.)
//
// Build: see firmware/README.md.

#include <Arduino.h>
#include "config.h"
#include "console.h"
#include "board.h"
#include "adc_inputs.h"
#include "can_bus.h"
#include "net.h"
#include "web.h"

static void printStatus() {
  con.printf("[%lus] VIN %.2f V |", (unsigned long)(millis() / 1000), ain[AIN_VIN].volts);
  for (auto &a : ain) {
    if (a.isInput) con.printf(" %s %.2f V %s%s |", a.name, a.volts, a.on ? "ON" : "off", a.errors ? " (ADC errors)" : "");
  }
  if (baro.ok) con.printf(" %.1f hPa %.1f C |", baro.hPa, baro.degC);
  else con.print(" MS5637 missing |");
  con.printf(" CPU %.1f C | MUTE %s | CAN rx %lu | ", temperatureRead(), mute.pressed ? "pressed" : "up", (unsigned long)can.rx);
  if (netState == NET_STA) con.printf("WiFi %s %d dBm %s\n", savedSsid.c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
  else if (netState == NET_AP) con.printf("hotspot %s %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
  else con.println("WiFi joining");
}

static void printHelp() {
  con.println("Keys: s status | l light | b buzzer | c CAN self-test | i I2C scan | w forget WiFi | r reboot | h help");
}

static void consoleLoop() {
  int c = con.read();
  switch (c) {
    case 's': printStatus(); break;
    case 'l': out.light = !out.light; applyOutputs(); con.printf("light %s\n", out.light ? "ON" : "off"); break;
    case 'b': out.buzzer = !out.buzzer; applyOutputs(); con.printf("buzzer %s\n", out.buzzer ? "ON" : "off"); break;
    case 'c': con.println(canSelfTest()); break;
    case 'i': i2cScan(); con.printf("I2C: %s\n", i2cFound.c_str()); break;
    case 'w': netSaveCreds("", ""); con.println("WiFi forgotten, rebooting"); scheduleReboot(500); break;
    case 'r': scheduleReboot(100); break;
    case 'h':
    case '?': printHelp(); break;
    default: break;
  }
}

// Status LED shows the Wi-Fi state, alarm LED follows the high-water input.
static void ledLoop() {
  if (!out.ledAuto) return;
  uint32_t t = millis();
  bool stat = netState == NET_STA ? (t % 2000) < 60        // short flash every 2 s: connected
            : netState == NET_AP ? (t % 1000) < 500        // slow even blink: setup hotspot
            : (t % 200) < 100;                             // fast blink: joining
  bool alm = ain[AIN_IN4].on;
  if (stat != out.ledStat || alm != out.ledAlm) {
    out.ledStat = stat;
    out.ledAlm = alm;
    applyOutputs();
  }
}

void setup() {
  boardBegin();   // outputs off first
  con.begin(115200);
  delay(300);
  con.printf("\n%s %s (built %s %s)\n", FW_NAME, FW_VERSION, __DATE__, __TIME__);
  con.printf("Reset: %s | I2C devices: %s\n", resetReasonText(), i2cFound.c_str());
  adcBegin();
  if (!canStart(TWAI_MODE_LISTEN_ONLY)) con.println("CAN: driver did not start");
  netBegin();
  webBegin();
  printHelp();
}

void loop() {
  static uint32_t tAdc = 0, tBaro = 0, tPrint = 0;
  uint32_t now = millis();
  if (now - tAdc >= 100) {
    tAdc = now;
    adcSample();
  }
  if (now - tBaro >= 1000) {
    tBaro = now;
    baroRead();
  }
  if (now - tPrint >= 5000) {
    tPrint = now;
    printStatus();
  }
  muteLoop();
  ledLoop();
  canPoll();
  netLoop();
  server.handleClient();
  consoleLoop();
  delay(2);
}
