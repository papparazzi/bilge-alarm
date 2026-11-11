// Bilge alarm Rev C.5 - alarm firmware for M/Y Lada Panas.
//
// IN1-IN3 watch the bilge pumps, IN4 the high-water float. The board raises alarms on the
// light, the buzzer and the alarm LED, counts every input and alarm since power-on, keeps an
// event log, and shows all of it on its own web page. It advertises temperature and pressure
// over Bluetooth for the Victron Cerbo GX.
//
// The alarm logic runs in its own task (core.h). This file is everything else: the web
// server, Wi-Fi, Bluetooth, the pressure sensor and the serial console.
//
// Build and settings: firmware/README.md.

#include <Arduino.h>
#include "config.h"
#include "console.h"
#include "settings.h"
#include "core.h"
#include "board.h"
#include "can_bus.h"
#include "net.h"
#include "ble.h"
#include "n2k.h"
#include "web.h"

static void printStatus() {
  static Snapshot s;
  coreSnapshot(s);
  con.printf("[%lus] VIN %.2f V |", (unsigned long)s.nowS, s.vin.volts);
  for (int c = 0; c < NCH; c++) con.printf(" %s %.1f V %s x%lu |", cfg.name[c], s.ch[c].volts, s.ch[c].on ? "ON" : "off", (unsigned long)s.cs[c].count);
  bool any = false;
  for (int a = 0; a < AL_N; a++) {
    if (!s.al[a].active) continue;
    con.printf(" ALARM %s%s |", alarmText(a).c_str(), s.al[a].muted ? " (muted)" : "");
    any = true;
  }
  if (!any) con.print(" no alarm |");
  if (baro.ok) con.printf(" %.1f hPa %.1f C | ", baro.hPa, baro.degC);
  else con.print(" MS5637 missing | ");
  if (netState == NET_STA) con.printf("WiFi %s %d dBm %s\n", savedSsid.c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
  else if (netState == NET_AP) con.printf("hotspot %s %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
  else con.println("WiFi joining");
}

static void printEvents() {
  static Event ev[EV_MAX];
  int n = coreEvents(ev, EV_MAX);
  uint32_t now = upS();
  for (int k = n - 1; k >= 0; k--) con.printf("  %lu s ago: %s\n", (unsigned long)(now - ev[k].t), eventText(ev[k]).c_str());
}

static void printHelp() { con.println("Keys: s status | e event log | m mute | c CAN self-test | w forget WiFi | r restart | h help"); }

static void consoleLoop() {
  switch (con.read()) {
    case 's': printStatus(); break;
    case 'e': printEvents(); break;
    case 'm': coreMute(); con.println("muted"); break;
    case 'c': con.println(canSelfTest()); break;
    case 'w': netSaveCreds("", ""); con.println("WiFi forgotten"); netRejoinSoon(200); break;
    case 'r': scheduleReboot(100); break;
    case 'h':
    case '?': printHelp(); break;
    default: break;
  }
}

void setup() {
  settingsLoad();
  coreBegin();   // outputs off, counters restored, alarm task running: before anything that can be slow
  con.begin(115200);
  delay(300);
  con.printf("\n%s %s (built %s %s)\n", FW_NAME, FW_VERSION, __DATE__, __TIME__);
  con.printf("Reset: %s | %s\n", resetReasonText(esp_reset_reason()), persistFresh ? "counters start from zero" : "counters carried over");
  boardBegin();
  n2kBegin();
  if (!can.running) con.println("CAN: driver did not start");
  netBegin();
  webBegin();
  printHelp();
}

void loop() {
  static uint32_t tBaro = 0, tPrint = 0;
  uint32_t now = millis();
  if (now - tBaro >= 1000) {
    tBaro = now;
    baroRead();
  }
  if (now - tPrint >= 10000) {
    tPrint = now;
    printStatus();
  }
  if (n2k.on) n2kLoop();
  else canPoll();
  canService();
  if (coreWifiResetReq) {   // MUTE held for 10 s: forget the network and open the setup hotspot
    coreWifiResetReq = false;
    con.println("WiFi: reset with the MUTE button");
    netSaveCreds("", "");
    netRejoinSoon(100);
  }
  netLoop();
  bleLoop();
  server.handleClient();
  consoleLoop();
  delay(2);
}
