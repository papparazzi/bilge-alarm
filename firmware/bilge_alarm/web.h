// Web UI and JSON API on port 80.
// Reading is open to anyone on the network. Changing anything (mute, tests, settings, Wi-Fi,
// firmware) asks for the login "admin" + the admin password once one is set on the page.
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <stdarg.h>
#include "config.h"
#include "console.h"
#include "settings.h"
#include "core.h"
#include "board.h"
#include "can_bus.h"
#include "net.h"
#include "ble.h"
#include "n2k.h"
#include "page.h"

static WebServer server(80);
static bool otaAllowed;

static const char *resetReasonText(int r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_BROWNOUT: return "brownout (supply dipped)";
    case ESP_RST_USB: return "USB";
    default: return "unknown";
  }
}

static String jsonEsc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if ((uint8_t)c < 0x20) {
      char b[8];
      snprintf(b, sizeof b, "\\u%04x", c);
      o += b;
    } else {
      o += c;
    }
  }
  return o;
}

static void jf(String &o, const char *fmt, ...) {
  char b[384];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  o += b;
}

static const char *tf(bool b) { return b ? "true" : "false"; }

static String statusJson() {
  static Snapshot s;   // too big for the stack; only the loop task comes here
  coreSnapshot(s);
  String o;
  o.reserve(5120);
  jf(o, "{\"fw\":\"%s\",\"version\":\"%s\",\"build\":\"%s %s\",\"up\":%lu,\"pon\":%lu,\"restarts\":%lu,\"reset\":\"%s\",", FW_NAME, FW_VERSION, __DATE__, __TIME__,
     (unsigned long)(millis() / 1000), (unsigned long)s.nowS, (unsigned long)s.restarts, resetReasonText(esp_reset_reason()));
  jf(o, "\"chip\":\"%s rev %d\",\"cores\":%d,\"flash_mb\":%lu,\"psram_kb\":%lu,\"heap\":%lu,\"heap_min\":%lu,\"sketch_kb\":%lu,\"sketch_max_kb\":%lu,",
     ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getChipCores(), (unsigned long)(ESP.getFlashChipSize() >> 20),
     (unsigned long)(ESP.getPsramSize() >> 10), (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
     (unsigned long)(ESP.getSketchSize() >> 10), (unsigned long)((ESP.getSketchSize() + ESP.getFreeSketchSpace()) >> 10));
  jf(o, "\"cpu_temp\":%.1f,\"on_v\":%.1f,\"off_v\":%.1f,\"login\":%s,", temperatureRead(), IN_ON_VOLTS, IN_OFF_VOLTS, tf(cfg.adminPass[0] != 0));

  // the alarm that matters most right now, for the banner
  int top = -1;
  bool anyUnmuted = false;
  for (int a = AL_N - 1; a >= 0; a--) {
    if (!s.al[a].active) continue;
    top = a;
    anyUnmuted |= !s.al[a].muted;
  }
  o += "\"alarm\":{\"active\":";
  o += tf(top >= 0);
  o += ",\"text\":\"" + jsonEsc(top >= 0 ? alarmText(top) : String("")) + "\",";
  jf(o, "\"sounding\":%s,\"unacked\":%s},\"inputs\":[", tf(anyUnmuted), tf(s.unacked));

  for (int c = 0; c < NCH; c++) {
    const ChanLive &L = s.ch[c];
    const ChanStat &S = s.cs[c];
    o += c ? ",{" : "{";
    jf(o, "\"name\":\"IN%d\",\"label\":\"%s\",\"pump\":%s,", c + 1, jsonEsc(cfg.name[c]).c_str(), tf(c < NPUMP));
    jf(o, "\"valid\":%s,\"v\":%.2f,\"mv\":%d,\"on\":%s,\"adc\":\"ADC%d\",\"reads\":%lu,\"err\":%lu,", tf(L.valid), L.volts, L.mv, tf(L.on),
       adcCh[c].unit == ADC_UNIT_1 ? 1 : 2, (unsigned long)L.adcReads, (unsigned long)L.adcErr);
    jf(o, "\"count\":%lu,\"total_s\":%lu,\"last_s\":%lu,\"longest_s\":%lu,\"last_on\":%lu,\"on_for\":%lu,\"starts_hour\":%u}", (unsigned long)S.count,
       (unsigned long)(S.totalS + (L.on ? s.nowS - L.onSinceS : 0)), (unsigned long)S.lastDurS, (unsigned long)S.longestS, (unsigned long)S.lastOnS,
       (unsigned long)(L.on ? s.nowS - L.onSinceS : 0), (unsigned)L.startsLastHour);
  }
  o += "],\"alarms\":[";
  for (int a = 0; a < AL_N; a++) {
    bool enabled = a == AL_HIGH_WATER || (a >= AL_LONG1 && a <= AL_LONG3 && cfg.longRunMin) || (a >= AL_CYC1 && a <= AL_CYC3 && cfg.cyclesPerHour) || (a == AL_LOW_VOLT && cfg.lowVolt > 0);
    o += a ? ",{" : "{";
    o += "\"id\":\"";
    o += ALARM_KEY[a];
    o += "\",\"text\":\"" + jsonEsc(alarmText(a)) + "\",";
    jf(o, "\"enabled\":%s,\"active\":%s,\"muted\":%s,\"for\":%lu,\"count\":%lu,\"last_on\":%lu}", tf(enabled), tf(s.al[a].active), tf(s.al[a].muted),
       (unsigned long)(s.al[a].active ? s.nowS - s.al[a].sinceS : 0), (unsigned long)s.as[a].count, (unsigned long)s.as[a].lastOnS);
  }
  jf(o, "],\"vin\":{\"valid\":%s,\"v\":%.2f,\"mv\":%d,\"min\":%.2f,\"max\":%.2f},\"hw_bypass\":%s,", tf(s.vin.valid), s.vin.volts, s.vin.mv, s.vin.minV, s.vin.maxV,
     tf(s.ch[CH_FLOAT].on));
  jf(o, "\"baro\":{\"present\":%s,\"ok\":%s,\"crc\":%s,\"hpa\":%.2f,\"temp\":%.2f,\"fails\":%lu},\"i2c\":\"%s\",", tf(ms5637.present), tf(baro.ok),
     tf(ms5637.crcOk), baro.hPa, baro.degC, (unsigned long)baro.fails, i2cFound.c_str());
  jf(o, "\"mute\":{\"pressed\":%s,\"presses\":%lu},", tf(s.mute.pressed), (unsigned long)s.mute.presses);
  jf(o, "\"out\":{\"light\":%d,\"buzzer\":%d,\"led_stat\":%d,\"led_alm\":%d,\"lamp_test\":%d},", s.out.light, s.out.buzzer, s.out.ledStat, s.out.ledAlm,
     s.out.lampTest);
  jf(o, "\"ble\":{\"enabled\":%s,\"advertising\":%s,\"failed\":%s,\"frames\":%lu,\"mac\":\"%s\"},", tf(cfg.bleEnable), tf(ble.advertising), tf(ble.failed),
     (unsigned long)ble.frames, ble.mac.c_str());

  twai_status_info_t st = {};
  if (can.running) twai_get_status_info(&st);
  const char *cs = !can.running ? "stopped" : st.state == TWAI_STATE_RUNNING ? "running" : st.state == TWAI_STATE_BUS_OFF ? "bus off" : st.state == TWAI_STATE_RECOVERING ? "recovering" : "stopped";
  jf(o, "\"can\":{\"running\":%s,\"mode\":\"%s\",\"kbps\":%d,\"state\":\"%s\",\"rx\":%lu,\"last_id\":%lu,\"last_age\":%lu,\"bus_err\":%lu,\"missed\":%lu,",
     tf(can.running), can.mode ? can.mode : "", CAN_BITRATE_KBPS, cs, (unsigned long)can.rx, (unsigned long)can.lastId,
     (unsigned long)((millis() - can.lastRxMs) / 1000), (unsigned long)st.bus_error_count, (unsigned long)st.rx_missed_count);
  jf(o, "\"n2k\":%s,\"on_bus\":%s,\"addr\":%u,\"sent\":%lu,\"send_fail\":%lu,\"bus_offs\":%lu,\"alerts\":%s,", tf(n2k.on), tf(can.txOkMs && millis() - can.txOkMs < 5000),
     n2k.addr, (unsigned long)n2k.sent, (unsigned long)n2k.sendFail, (unsigned long)can.busOffs, tf(cfg.n2kAlerts));
  o += "\"selftest\":\"" + jsonEsc(can.selfTest) + "\",\"pgns\":[";
  for (int i = 0; i < can.nSeen; i++) jf(o, "%s{\"pgn\":%lu,\"src\":%u,\"n\":%lu}", i ? "," : "", (unsigned long)can.seen[i].pgn, can.seen[i].src, (unsigned long)can.seen[i].count);
  o += "]},";

  const char *mode = netState == NET_AP ? "AP" : netState == NET_STA ? "STA" : "CONNECTING";
  String ip = netState == NET_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  o += "\"wifi\":{\"mode\":\"";
  o += mode;
  o += "\",\"ssid\":\"" + jsonEsc(savedSsid) + "\",\"saved\":\"" + jsonEsc(savedSsid) + "\",\"ap_ssid\":\"" + jsonEsc(apSsid) + "\",";
  jf(o, "\"rssi\":%d,\"channel\":%d,\"clients\":%d,\"ip\":\"%s\",\"host\":\"%s\",\"mac\":\"%s\",\"joins\":%lu,\"fallbacks\":%lu}}", netState == NET_STA ? WiFi.RSSI() : 0,
     (int)WiFi.channel(), netState == NET_AP ? WiFi.softAPgetStationNum() : 0, ip.c_str(), HOSTNAME, WiFi.macAddress().c_str(), (unsigned long)netJoins,
     (unsigned long)netFallbacks);
  return o;
}

static String eventText(const Event &e) {
  char b[96];
  switch (e.type) {
    case EV_BOOT:
      if (e.val == 0) snprintf(b, sizeof b, "Powered on");
      else snprintf(b, sizeof b, "Restarted (%s)", resetReasonText(e.idx));
      return b;
    case EV_INPUT_ON:
      return String(cfg.name[e.idx < NCH ? e.idx : 0]) + " on";
    case EV_INPUT_OFF: {
      String s = String(cfg.name[e.idx < NCH ? e.idx : 0]) + " off after ";
      if (e.val == 65535) s += "more than 18 h";
      else if (e.val >= 3600) s += String(e.val / 3600) + " h " + String(e.val % 3600 / 60) + " min";
      else if (e.val >= 60) s += String(e.val / 60) + " min " + String(e.val % 60) + " s";
      else s += String(e.val) + " s";
      return s;
    }
    case EV_ALARM_ON: return "ALARM: " + alarmText(e.idx < AL_N ? e.idx : 0);
    case EV_ALARM_OFF: return "Alarm cleared: " + alarmText(e.idx < AL_N ? e.idx : 0);
    case EV_MUTE: return "Muted / acknowledged";
    case EV_COUNTERS_RESET: return "Counters and log reset";
    case EV_WIFI_RESET: return "Wi-Fi reset with the MUTE button";
    default: return "?";
  }
}

static String logJson() {
  static Event ev[EV_MAX];
  int n = coreEvents(ev, EV_MAX);
  String o;
  o.reserve(64 + n * 96);
  jf(o, "{\"pon\":%lu,\"events\":[", (unsigned long)upS());
  for (int k = 0; k < n; k++) {
    jf(o, "%s{\"t\":%lu,\"type\":%u,\"alarm\":%s,\"text\":\"", k ? "," : "", (unsigned long)ev[k].t, ev[k].type, tf(ev[k].type == EV_ALARM_ON));
    o += jsonEsc(eventText(ev[k])) + "\"}";
  }
  o += "]}";
  return o;
}

static String settingsJson() {
  String o = "{\"names\":[";
  for (int i = 0; i < NCH; i++) o += String(i ? "," : "") + "\"" + jsonEsc(cfg.name[i]) + "\"";
  jf(o, "],\"float_delay\":%u,\"long_run\":%u,\"cycles\":%u,\"low_volt\":%.1f,\"light_on_pump\":%s,\"buzz_long\":%s,\"buzz_cyc\":%s,\"buzz_low\":%s,\"ble\":%s,\"vin_cal\":%.3f,\"login\":%s,",
     cfg.floatDelayS, cfg.longRunMin, cfg.cyclesPerHour, cfg.lowVolt, tf(cfg.lightOnPump), tf(cfg.buzzLongRun), tf(cfg.buzzCycles), tf(cfg.buzzLowVolt),
     tf(cfg.bleEnable), cfg.vinCal, tf(cfg.adminPass[0] != 0));
  jf(o, "\"n2k\":%s,\"n2k_alerts\":%s,\"n2k_bank\":%u,\"n2k_batt\":%u,\"n2k_temp\":%u,\"n2k_src\":%u}", tf(cfg.n2kEnable), tf(cfg.n2kAlerts), cfg.n2kBankInst, cfg.n2kBattInst,
     cfg.n2kTempInst, cfg.n2kTempSrc);
  return o;
}

static void sendJson(const String &body, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

// True when the caller may change things. Otherwise the browser is asked for the login.
static bool authed() {
  if (!cfg.adminPass[0] || server.authenticate("admin", cfg.adminPass)) return true;
  server.requestAuthentication(BASIC_AUTH, "Bilge Alarm");
  return false;
}

static void handleSettingsSave() {
  if (!authed()) return;
  Settings s = cfg;
  char key[4];
  for (int i = 0; i < NCH; i++) {
    snprintf(key, sizeof key, "n%d", i);
    if (!server.hasArg(key)) continue;
    String v = server.arg(key);
    v.trim();
    if (v.length()) strlcpy(s.name[i], v.c_str(), sizeof s.name[i]);
  }
  if (server.hasArg("float_delay")) s.floatDelayS = server.arg("float_delay").toInt();
  if (server.hasArg("long_run")) s.longRunMin = server.arg("long_run").toInt();
  if (server.hasArg("cycles")) s.cyclesPerHour = server.arg("cycles").toInt();
  if (server.hasArg("low_volt")) s.lowVolt = server.arg("low_volt").toFloat();
  if (server.hasArg("vin_cal")) s.vinCal = server.arg("vin_cal").toFloat();
  if (server.hasArg("n2k_bank")) s.n2kBankInst = constrain(server.arg("n2k_bank").toInt(), 0, 252);
  if (server.hasArg("n2k_batt")) s.n2kBattInst = constrain(server.arg("n2k_batt").toInt(), 0, 252);
  if (server.hasArg("n2k_temp")) s.n2kTempInst = constrain(server.arg("n2k_temp").toInt(), 0, 252);
  if (server.hasArg("n2k_src")) s.n2kTempSrc = constrain(server.arg("n2k_src").toInt(), 0, 15);
  if (server.hasArg("flags")) {   // the page sends all checkboxes together
    s.lightOnPump = server.arg("light_on_pump") == "1";
    s.buzzLongRun = server.arg("buzz_long") == "1";
    s.buzzCycles = server.arg("buzz_cyc") == "1";
    s.buzzLowVolt = server.arg("buzz_low") == "1";
    s.bleEnable = server.arg("ble") == "1";
    s.n2kEnable = server.arg("n2k") == "1";
    s.n2kAlerts = server.arg("n2k_alerts") == "1";
  }
  if (server.hasArg("pass_set")) {
    String pw = server.arg("pass");
    if (pw.length() >= sizeof s.adminPass) {
      sendJson("{\"ok\":false,\"msg\":\"The admin password can be at most 31 characters.\"}", 400);
      return;
    }
    strlcpy(s.adminPass, pw.c_str(), sizeof s.adminPass);
  }
  coreApplySettings(s);
  settingsSave();
  sendJson("{\"ok\":true,\"msg\":\"Saved.\"}");
}

static void handleScan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) { sendJson("{\"scanning\":true}"); return; }
  if (n == WIFI_SCAN_FAILED || server.hasArg("start")) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);   // asynchronous
    sendJson("{\"scanning\":true}");
    return;
  }
  String o = "{\"scanning\":false,\"nets\":[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;   // results are sorted strongest first: keep the first of each name
    for (int k = 0; k < i && !dup; k++) dup = WiFi.SSID(k) == s;
    if (dup) continue;
    jf(o, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}", first ? "" : ",", jsonEsc(s).c_str(), (int)WiFi.RSSI(i), tf(WiFi.encryptionType(i) == WIFI_AUTH_OPEN));
    first = false;
  }
  o += "]}";
  sendJson(o);
}

static void handleWifiSave() {
  if (!authed()) return;
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (!ssid.length() || ssid.length() > 32) { sendJson("{\"ok\":false,\"msg\":\"Enter a network name (up to 32 characters).\"}", 400); return; }
  if (pass.length() && (pass.length() < 8 || pass.length() > 63)) { sendJson("{\"ok\":false,\"msg\":\"The password must be 8 to 63 characters, or empty for an open network.\"}", 400); return; }
  netSaveCreds(ssid, pass);
  sendJson("{\"ok\":true,\"msg\":\"Saved. Joining the network now. If it cannot connect, the setup hotspot comes back after about 20 seconds.\"}");
  netRejoinSoon(1500);
}

static void handleUpdateData() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaAllowed = !cfg.adminPass[0] || server.authenticate("admin", cfg.adminPass);
    if (!otaAllowed) return;
    con.printf("OTA: receiving %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(con);
  } else if (!otaAllowed) {
    return;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(con);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) con.printf("OTA: %u bytes written\n", (unsigned)up.totalSize);
    else Update.printError(con);
  }
}

static void webBegin() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/status", HTTP_GET, []() { sendJson(statusJson()); });
  server.on("/api/log", HTTP_GET, []() { sendJson(logJson()); });
  server.on("/api/settings", HTTP_GET, []() { sendJson(settingsJson()); });
  server.on("/api/settings", HTTP_POST, handleSettingsSave);
  server.on("/api/mute", HTTP_POST, []() {   // open on purpose: silencing must never need a password
    coreMute();
    sendJson("{\"ok\":true}");
  });
  server.on("/api/test", HTTP_POST, []() {
    if (!authed()) return;
    String n = server.arg("name");
    coreTestOutput(n == "light" || n == "both", n == "buzzer" || n == "both");
    sendJson("{\"ok\":true}");
  });
  server.on("/api/counters/reset", HTTP_POST, []() {
    if (!authed()) return;
    coreResetCounters();
    sendJson("{\"ok\":true}");
  });
  server.on("/api/can/selftest", HTTP_POST, []() {
    if (!authed()) return;
    sendJson("{\"result\":\"" + jsonEsc(canSelfTest()) + "\"}");
  });
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/forget", HTTP_POST, []() {
    if (!authed()) return;
    netSaveCreds("", "");
    sendJson("{\"ok\":true,\"msg\":\"Forgotten. Opening the setup hotspot.\"}");
    netRejoinSoon(1500);
  });
  server.on("/api/reboot", HTTP_POST, []() {
    if (!authed()) return;
    sendJson("{\"ok\":true}");
    scheduleReboot(500);
  });
  server.on("/update", HTTP_POST, []() {
    if (!otaAllowed) {
      server.requestAuthentication(BASIC_AUTH, "Bilge Alarm");
      return;
    }
    bool ok = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "text/plain", ok ? "Firmware written. Restarting. Counters and the log are kept." : "Update failed. The old firmware is still running.");
    if (ok) scheduleReboot(1000);
  }, handleUpdateData);
  server.onNotFound([]() {
    if (netState == NET_AP) {   // captive portal: any other address lands on the setup page
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "not found");
    }
  });
  server.begin();
}
