// Web UI and JSON API on port 80. No login: keep it on the boat's own network.
#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <stdarg.h>
#include "config.h"
#include "console.h"
#include "board.h"
#include "adc_inputs.h"
#include "can_bus.h"
#include "net.h"
#include "page.h"

static WebServer server(80);

static const char *resetReasonText() {
  switch (esp_reset_reason()) {
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
  char b[320];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  o += b;
}

static String statusJson() {
  String o;
  o.reserve(3072);
  jf(o, "{\"fw\":\"%s\",\"version\":\"%s\",\"build\":\"%s %s\",\"uptime\":%lu,\"reset\":\"%s\",", FW_NAME, FW_VERSION, __DATE__, __TIME__,
     (unsigned long)(millis() / 1000), resetReasonText());
  jf(o, "\"chip\":\"%s rev %d\",\"cores\":%d,\"flash_mb\":%lu,\"psram_kb\":%lu,\"heap\":%lu,\"heap_min\":%lu,\"sketch_kb\":%lu,\"sketch_max_kb\":%lu,",
     ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getChipCores(), (unsigned long)(ESP.getFlashChipSize() >> 20),
     (unsigned long)(ESP.getPsramSize() >> 10), (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
     (unsigned long)(ESP.getSketchSize() >> 10), (unsigned long)((ESP.getSketchSize() + ESP.getFreeSketchSpace()) >> 10));
  jf(o, "\"cpu_temp\":%.1f,\"on_v\":%.1f,\"off_v\":%.1f,\"inputs\":[", temperatureRead(), IN_ON_VOLTS, IN_OFF_VOLTS);
  bool first = true;
  for (auto &a : ain) {
    if (!a.isInput) continue;
    jf(o, "%s{\"name\":\"%s\",\"label\":\"%s\",\"valid\":%s,\"v\":%.2f,\"mv\":%d,\"on\":%s,\"adc\":\"ADC%d\",\"reads\":%lu,\"err\":%lu}", first ? "" : ",",
       a.name, a.label, a.valid ? "true" : "false", a.volts, a.mv, a.on ? "true" : "false", a.unit == ADC_UNIT_1 ? 1 : 2, (unsigned long)a.reads,
       (unsigned long)a.errors);
    first = false;
  }
  AnalogIn &vin = ain[AIN_VIN];
  jf(o, "],\"vin\":{\"valid\":%s,\"v\":%.2f,\"mv\":%d},\"hw_bypass\":%s,", vin.valid ? "true" : "false", vin.volts, vin.mv,
     ain[AIN_IN4].on ? "true" : "false");
  jf(o, "\"baro\":{\"present\":%s,\"ok\":%s,\"crc\":%s,\"hpa\":%.2f,\"temp\":%.2f,\"fails\":%lu},\"i2c\":\"%s\",", ms5637.present ? "true" : "false",
     baro.ok ? "true" : "false", ms5637.crcOk ? "true" : "false", baro.hPa, baro.degC, (unsigned long)baro.fails, i2cFound.c_str());
  jf(o, "\"mute\":{\"pressed\":%s,\"presses\":%lu},", mute.pressed ? "true" : "false", (unsigned long)mute.presses);
  jf(o, "\"out\":{\"light\":%d,\"buzzer\":%d,\"led_stat\":%d,\"led_alm\":%d,\"led_auto\":%d,\"lamp_test\":%d},", out.light, out.buzzer, out.ledStat,
     out.ledAlm, out.ledAuto, out.lampTest);

  twai_status_info_t st = {};
  if (can.running) twai_get_status_info(&st);
  const char *cs = !can.running ? "stopped" : st.state == TWAI_STATE_RUNNING ? "running" : st.state == TWAI_STATE_BUS_OFF ? "bus off" : st.state == TWAI_STATE_RECOVERING ? "recovering" : "stopped";
  jf(o, "\"can\":{\"running\":%s,\"mode\":\"%s\",\"kbps\":%d,\"state\":\"%s\",\"rx\":%lu,\"last_id\":%lu,\"last_age\":%lu,\"bus_err\":%lu,\"missed\":%lu,",
     can.running ? "true" : "false", can.mode ? can.mode : "", CAN_BITRATE_KBPS, cs, (unsigned long)can.rx, (unsigned long)can.lastId,
     (unsigned long)((millis() - can.lastRxMs) / 1000), (unsigned long)st.bus_error_count, (unsigned long)st.rx_missed_count);
  o += "\"selftest\":\"" + jsonEsc(can.selfTest) + "\",\"pgns\":[";
  for (int i = 0; i < can.nSeen; i++) jf(o, "%s{\"pgn\":%lu,\"src\":%u,\"n\":%lu}", i ? "," : "", (unsigned long)can.seen[i].pgn, can.seen[i].src, (unsigned long)can.seen[i].count);
  o += "]},";

  const char *mode = netState == NET_AP ? "AP" : netState == NET_STA ? "STA" : "CONNECTING";
  String ip = netState == NET_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  o += "\"wifi\":{\"mode\":\"";
  o += mode;
  o += "\",\"ssid\":\"" + jsonEsc(savedSsid) + "\",\"saved\":\"" + jsonEsc(savedSsid) + "\",\"ap_ssid\":\"" + jsonEsc(apSsid) + "\",";
  jf(o, "\"rssi\":%d,\"channel\":%d,\"clients\":%d,\"ip\":\"%s\",\"host\":\"%s\",\"mac\":\"%s\"}}", netState == NET_STA ? WiFi.RSSI() : 0, (int)WiFi.channel(),
     netState == NET_AP ? WiFi.softAPgetStationNum() : 0, ip.c_str(), HOSTNAME, WiFi.macAddress().c_str());
  return o;
}

static void sendJson(const String &body, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void handleOutput() {
  String n = server.arg("name");
  bool v = server.arg("state") == "1";
  if (n == "light") out.light = v;
  else if (n == "buzzer") out.buzzer = v;
  else if (n == "led_stat") { out.ledAuto = false; out.ledStat = v; }
  else if (n == "led_alm") { out.ledAuto = false; out.ledAlm = v; }
  else if (n == "led_auto") out.ledAuto = true;
  else { sendJson("{\"ok\":false,\"msg\":\"unknown output\"}", 400); return; }
  applyOutputs();
  sendJson("{\"ok\":true}");
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
    jf(o, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}", first ? "" : ",", jsonEsc(s).c_str(), (int)WiFi.RSSI(i),
       WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
    first = false;
  }
  o += "]}";
  sendJson(o);
}

static void handleWifiSave() {
  String ssid = server.arg("ssid"), pass = server.arg("pass");
  if (!ssid.length() || ssid.length() > 32) { sendJson("{\"ok\":false,\"msg\":\"Enter a network name (up to 32 characters).\"}", 400); return; }
  if (pass.length() && (pass.length() < 8 || pass.length() > 63)) { sendJson("{\"ok\":false,\"msg\":\"The password must be 8 to 63 characters, or empty for an open network.\"}", 400); return; }
  netSaveCreds(ssid, pass);
  sendJson("{\"ok\":true,\"msg\":\"Saved. Rebooting to join the network. If it cannot connect, the setup hotspot comes back after about 20 seconds.\"}");
  scheduleReboot(1500);
}

static void handleUpdateData() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    out.light = out.buzzer = false;
    applyOutputs();
    con.printf("OTA: receiving %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(con);
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
  server.on("/api/output", HTTP_POST, handleOutput);
  server.on("/api/can/selftest", HTTP_POST, []() { sendJson("{\"result\":\"" + jsonEsc(canSelfTest()) + "\"}"); });
  server.on("/api/i2c/scan", HTTP_POST, []() { i2cScan(); sendJson("{\"found\":\"" + i2cFound + "\"}"); });
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/forget", HTTP_POST, []() {
    netSaveCreds("", "");
    sendJson("{\"ok\":true,\"msg\":\"Forgotten. Rebooting into the setup hotspot.\"}");
    scheduleReboot(1500);
  });
  server.on("/api/reboot", HTTP_POST, []() {
    sendJson("{\"ok\":true}");
    scheduleReboot(500);
  });
  server.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    server.sendHeader("Connection", "close");
    server.send(ok ? 200 : 500, "text/plain", ok ? "Firmware written. Rebooting." : "Update failed. The old firmware is still running.");
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
