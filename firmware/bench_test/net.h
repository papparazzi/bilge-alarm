// Wi-Fi: join the saved network; if there is none or it fails, open a setup
// hotspot with a captive portal so a network can be chosen from the web UI.
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "esp_mac.h"
#include "config.h"
#include "console.h"

enum NetState { NET_OFF, NET_CONNECTING, NET_STA, NET_AP };

static NetState netState = NET_OFF;
static String savedSsid, savedPass, apSsid;
static DNSServer dnsServer;
static uint32_t netT0, staLastOk, apIdleSince, rebootAt;

static void scheduleReboot(uint32_t inMs) {
  rebootAt = millis() + inMs;
  if (!rebootAt) rebootAt = 1;
}

static void netLoadCreds() {
  Preferences p;
  p.begin("wifi", true);
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

static void netStartMdns() {
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

static void netStartSta() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());
  netState = NET_CONNECTING;
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
  dnsServer.start(53, "*", WiFi.softAPIP());   // every name resolves to us: phones open the setup page
  netStartMdns();
  netState = NET_AP;
  apIdleSince = millis();
  con.printf("WiFi: setup hotspot \"%s\" at http://%s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

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
  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) ESP.restart();
  switch (netState) {
    case NET_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        netState = NET_STA;
        staLastOk = millis();
        netStartMdns();
        con.printf("WiFi: connected, http://%s or http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
      } else if (millis() - netT0 > STA_CONNECT_TIMEOUT_MS) {
        con.println("WiFi: could not join the saved network");
        netStartAp();
      }
      break;
    case NET_STA:
      if (WiFi.status() == WL_CONNECTED) {
        staLastOk = millis();
      } else if (millis() - staLastOk > STA_LOST_REBOOT_MS) {
        con.println("WiFi: connection lost, rebooting");
        ESP.restart();
      }
      break;
    case NET_AP:
      dnsServer.processNextRequest();
      if (WiFi.softAPgetStationNum() > 0) {
        apIdleSince = millis();
      } else if (savedSsid.length() && millis() - apIdleSince > AP_RETRY_STA_MS) {
        con.println("WiFi: hotspot idle, rebooting to retry the saved network");
        ESP.restart();
      }
      break;
    default:
      break;
  }
}
