// Wi-Fi: join the saved network; if there is none or it fails, open a setup hotspot with a
// captive portal so a network can be chosen on the web page.
// Nothing here restarts the board: the alarm core and its counters keep running through
// every Wi-Fi change.
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "esp_mac.h"
#include "config.h"
#include "console.h"
#include "core.h"

enum NetState { NET_OFF, NET_CONNECTING, NET_STA, NET_AP };

static NetState netState = NET_OFF;
static String savedSsid, savedPass, apSsid;
static DNSServer dnsServer;
static bool dnsRunning;
static uint32_t netT0, staLastOk, apIdleSince, rebootAt, rejoinAt;
static uint32_t netJoins, netFallbacks;   // since this boot

static void scheduleReboot(uint32_t inMs) { rebootAt = (millis() + inMs) | 1; }

static void netLoadCreds() {
  Preferences p;
  if (!p.begin("wifi", true)) return;
  savedSsid = p.getString("ssid", "");
  savedPass = p.getString("pass", "");
  p.end();
}

static void netSaveCreds(const String &ssid, const String &pass) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
  savedSsid = ssid;
  savedPass = pass;
}

static void netRestartMdns() {
  MDNS.end();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

static void netStopAp() {
  if (dnsRunning) {
    dnsServer.stop();
    dnsRunning = false;
  }
  WiFi.softAPdisconnect(true);
}

static void netStartSta() {
  netStopAp();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());
  netState = NET_CONNECTING;
  netLed = 0;
  netT0 = millis();
  con.printf("WiFi: joining \"%s\"\n", savedSsid.c_str());
}

static void netStartAp() {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP_STA);   // station side stays idle; it is only there so networks can be scanned
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD);
  delay(100);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsRunning = dnsServer.start(53, "*", WiFi.softAPIP());   // every name resolves to us: phones open the setup page
  netRestartMdns();
  netState = NET_AP;
  netLed = 2;
  apIdleSince = millis();
  netFallbacks++;
  con.printf("WiFi: setup hotspot \"%s\" at http://%s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

// Called by the web page after new details were saved: answer first, switch a moment later.
static void netRejoinSoon(uint32_t inMs) { rejoinAt = (millis() + inMs) | 1; }

static void netBegin() {
  WiFi.persistent(false);   // credentials live in our own NVS namespace
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);   // WiFi.macAddress() reads zeros before the radio is started
  char b[32];
  snprintf(b, sizeof b, "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
  apSsid = b;
  netLoadCreds();
  if (savedSsid.length()) netStartSta();
  else netStartAp();
}

static void netLoop() {
  uint32_t now = millis();
  if (rebootAt && (int32_t)(now - rebootAt) >= 0) ESP.restart();
  if (rejoinAt && (int32_t)(now - rejoinAt) >= 0) {
    rejoinAt = 0;
    if (savedSsid.length()) netStartSta();
    else if (netState != NET_AP) netStartAp();
    return;
  }
  switch (netState) {
    case NET_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        netState = NET_STA;
        netLed = 1;
        staLastOk = now;
        netJoins++;
        netRestartMdns();
        con.printf("WiFi: connected, http://%s or http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
      } else if (now - netT0 > STA_CONNECT_TIMEOUT_MS) {
        con.println("WiFi: could not join the saved network");
        netStartAp();
      }
      break;
    case NET_STA:
      if (WiFi.status() == WL_CONNECTED) {
        staLastOk = now;
        netLed = 1;
      } else {
        netLed = 0;
        if (now - staLastOk > STA_LOST_TO_AP_MS) {
          con.println("WiFi: connection lost");
          netStartAp();
        }
      }
      break;
    case NET_AP:
      if (dnsRunning) dnsServer.processNextRequest();
      if (WiFi.softAPgetStationNum() > 0) {
        apIdleSince = now;
      } else if (savedSsid.length() && now - apIdleSince > AP_RETRY_STA_MS) {
        con.println("WiFi: hotspot idle, trying the saved network again");
        netStartSta();
      }
      break;
    default:
      break;
  }
}
