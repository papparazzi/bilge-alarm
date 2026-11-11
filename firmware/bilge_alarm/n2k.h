// NMEA 2000 output, using the ttlappalainen/NMEA2000 library with our own CAN driver (can_bus.h).
//
// What is sent, and why (docs/FIRMWARE_ARCHITECTURE.md has the research):
//   127501 Binary Switch Bank Status  switch 1-3 = pumps running, 4 = float up, 5 = any alarm
//                                     (Signal K and Maretron-style displays; the Axiom ignores it)
//   127508 Battery Status             supply voltage (the Axiom shows it and can alarm on it)
//   130310 Environmental Parameters   barometric pressure (the Axiom's pressure source)
//   130314 Actual Pressure            the same, for everything else
//   130316 Temperature, Extended      board temperature
//   126983 Alert + 126985 Alert Text  every active alarm (Axiom, LightHouse 4 v4.8.164 or later)
//   126984 Alert Response (received)  acknowledge or silence from a display = MUTE
//
// The library has no functions for the three alert PGNs, so they are built by hand here from
// the field lists in the canboat PGN database. They have NOT been tried against a real display.
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <NMEA2000.h>
#include <N2kMessages.h>
#include "esp_mac.h"
#include "config.h"
#include "console.h"
#include "settings.h"
#include "core.h"
#include "board.h"
#include "can_bus.h"

class BilgeN2k : public tNMEA2000 {
protected:
  bool CANOpen() override { return can.running; }
  bool CANSendFrame(unsigned long id, unsigned char len, const unsigned char *buf, bool) override { return canSend(id, len, buf); }
  bool CANGetFrame(unsigned long &id, unsigned char &len, unsigned char *buf) override {
    uint32_t i;
    uint8_t l;
    if (!canReceive(i, l, buf)) return false;
    id = i;
    len = l;
    return true;
  }
};

enum { N2K_ALERT_IDLE, N2K_ALERT_ACTIVE, N2K_ALERT_CLEARING };

static BilgeN2k N2K;
static struct {
  bool on;
  uint8_t addr;
  uint8_t sid;
  uint32_t sent, sendFail;   // messages handed to the library / refused by it, since this boot
  uint32_t tSwitch, tBatt, tEnv;
  uint64_t lastBank;
  uint64_t name;             // our 64-bit NMEA 2000 NAME
  struct {
    uint8_t phase;
    uint8_t normalLeft;      // "back to normal" messages still to send
    uint32_t tAlert, tText;
    uint64_t ackBy;          // NAME of the display that acknowledged, or all ones
  } al[AL_N];
} n2k;

static const unsigned long N2K_TX_PGNS[] PROGMEM = {127501L, 127508L, 130310L, 130314L, 130316L, 126983L, 126985L, 0};
static const unsigned long N2K_RX_PGNS[] PROGMEM = {126984L, 0};

static bool n2kSend(const tN2kMsg &m) {
  bool ok = N2K.SendMsg(m);
  if (ok) n2k.sent++;
  else n2k.sendFail++;
  return ok;
}

// ---------------------------------------------------------------- alerts (PGN 126983, 126985, 126984)
// Alert type: 1 emergency alarm, 2 alarm, 5 warning, 8 caution. Category: 0 navigational, 1 technical.
static uint8_t n2kAlertTypeCat(uint8_t id) { return (id == AL_HIGH_WATER ? 2 : 5) | (1 << 4); }

// The part that 126983, 126984 and 126985 share: which alert this is.
static void n2kAlertHeader(tN2kMsg &m, uint8_t id, uint8_t occurrence) {
  m.AddByte(n2kAlertTypeCat(id));
  m.AddByte(1);               // alert system: ours
  m.AddByte(0);               // alert sub-system
  m.Add2ByteUInt(id + 1);     // alert ID 1..8
  m.AddUInt64(n2k.name);      // data source: this device
  m.AddByte(0);               // data source instance
  m.AddByte(id);              // data source index
  m.AddByte(occurrence);
}

static void n2kSendAlert(uint8_t id, uint8_t occurrence, bool active, bool muted) {
  tN2kMsg m;
  m.SetPGN(126983L);
  m.Priority = 2;
  m.Destination = 255;
  n2kAlertHeader(m, id, occurrence);
  // bit 0 silenced, 1 acknowledged, 2 escalated, 3 silence supported, 4 acknowledge supported, 5 escalation supported
  m.AddByte((muted ? 0x02 : 0x00) | 0x08 | 0x10 | 0xC0);
  m.AddUInt64(muted ? (n2k.al[id].ackBy != UINT64_MAX ? n2k.al[id].ackBy : n2k.name) : UINT64_MAX);
  uint8_t threshold = !active ? 0 : id == AL_LOW_VOLT ? 3 : 1;   // normal / low threshold exceeded / threshold exceeded
  m.AddByte(1 | threshold << 4);                                  // trigger: automatic
  m.AddByte(id == AL_HIGH_WATER ? 1 : 100);                       // priority
  m.AddByte(!active ? 1 : muted ? 4 : 2);                         // state: normal / acknowledged / active
  n2kSend(m);
}

static void n2kSendAlertText(uint8_t id, uint8_t occurrence) {
  tN2kMsg m;
  m.SetPGN(126985L);
  m.Priority = 2;
  m.Destination = 255;
  n2kAlertHeader(m, id, occurrence);
  m.AddByte(0);   // language: English
  String text = alarmText(id);
  m.AddVarStr(text.c_str());
  m.AddVarStr("Bilge alarm");
  n2kSend(m);
}

// A display acknowledges or silences one of our alerts: same as pressing MUTE.
static void n2kOnMsg(const tN2kMsg &m) {
  if (m.PGN != 126984L) return;
  int i = 0;
  m.GetByte(i);                        // type and category
  m.GetByte(i);                        // system
  m.GetByte(i);                        // sub-system
  uint16_t alertId = m.Get2ByteUInt(i);
  uint64_t source = m.GetUInt64(i);
  m.GetByte(i);                        // instance
  m.GetByte(i);                        // index
  m.GetByte(i);                        // occurrence
  uint64_t by = m.GetUInt64(i);
  uint8_t command = m.GetByte(i) & 0x03;   // 0 acknowledge, 1 temporary silence, 2/3 test off/on
  if (source != n2k.name || alertId < 1 || alertId > AL_N || command > 1) return;
  n2k.al[alertId - 1].ackBy = by;
  coreMute();
}

static void n2kAlerts(const Snapshot &s, uint32_t now) {
  for (uint8_t id = 0; id < AL_N; id++) {
    auto &A = n2k.al[id];
    uint8_t occurrence = s.as[id].count ? (s.as[id].count - 1) % 250 + 1 : 1;
    if (s.al[id].active) {
      if (A.phase != N2K_ALERT_ACTIVE) {
        A.phase = N2K_ALERT_ACTIVE;
        A.tAlert = A.tText = 0;
        A.ackBy = UINT64_MAX;
      }
      if (!A.tText || now - A.tText >= 10000) {   // the text first: displays ignore an alert they have no text for
        A.tText = now | 1;
        n2kSendAlertText(id, occurrence);
      }
      if (!A.tAlert || now - A.tAlert >= 1000) {
        A.tAlert = now | 1;
        n2kSendAlert(id, occurrence, true, s.al[id].muted);
      }
    } else if (A.phase == N2K_ALERT_ACTIVE) {
      A.phase = N2K_ALERT_CLEARING;
      A.normalLeft = 3;
      A.tAlert = 0;
    }
    if (A.phase == N2K_ALERT_CLEARING && (!A.tAlert || now - A.tAlert >= 1000)) {
      A.tAlert = now | 1;
      n2kSendAlert(id, occurrence, false, false);
      if (--A.normalLeft == 0) A.phase = N2K_ALERT_IDLE;
    }
  }
}

// ---------------------------------------------------------------- set-up and the periodic messages
static void n2kBegin() {
  n2k.on = cfg.n2kEnable;
  if (!n2k.on) {
    canStart(can.homeMode = TWAI_MODE_LISTEN_ONLY);
    return;
  }
  canStart(can.homeMode = TWAI_MODE_NORMAL);

  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  static char serial[16];
  snprintf(serial, sizeof serial, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  uint32_t unique = ((uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5]) & 0x1FFFFF;

  Preferences p;
  uint8_t addr = 35;
  if (p.begin("n2k", true)) {
    addr = p.getUChar("addr", addr);
    p.end();
  }
  N2K.SetN2kCANSendFrameBufSize(120);
  N2K.SetProductInformation(serial, 100, "Bilge Alarm Rev C.5", FW_VERSION, "Rev C.5", 1);   // load equivalency 1 = 50 mA
  N2K.SetDeviceInformation(unique, 130, 30, 2046);   // function 130 binary event monitor, class 30 electrical distribution, 2046 = no manufacturer code
  N2K.SetMode(tNMEA2000::N2km_ListenAndNode, addr);
  N2K.EnableForward(false);
  N2K.ExtendTransmitMessages(N2K_TX_PGNS);
  N2K.ExtendReceiveMessages(N2K_RX_PGNS);
  N2K.SetMsgHandler(n2kOnMsg);
  N2K.Open();
  n2k.name = N2K.GetDeviceInformation().GetName();
  n2k.addr = addr;
  for (auto &A : n2k.al) A.ackBy = UINT64_MAX;
  con.printf("NMEA 2000: on, starting at address %u\n", addr);
}

static void n2kLoop() {
  if (!n2k.on) return;
  N2K.ParseMessages();
  if (N2K.ReadResetAddressChanged()) {   // another device wanted our address: remember the new one
    n2k.addr = N2K.GetN2kSource();
    Preferences p;
    p.begin("n2k", false);
    p.putUChar("addr", n2k.addr);
    p.end();
  }
  uint32_t now = millis();
  static Snapshot s;
  static uint32_t tSnap = 0;
  if (now - tSnap < 200) return;
  tSnap = now;
  coreSnapshot(s);

  tN2kBinaryStatus bank;
  N2kResetBinaryStatus(bank);
  bool any = false;
  for (auto &A : s.al) any |= A.active;
  for (int c = 0; c < NCH; c++) N2kSetStatusBinaryOnStatus(bank, s.ch[c].on ? N2kOnOff_On : N2kOnOff_Off, c + 1);
  N2kSetStatusBinaryOnStatus(bank, any ? N2kOnOff_On : N2kOnOff_Off, 5);
  tN2kMsg m;
  if (bank != n2k.lastBank || now - n2k.tSwitch >= 2000) {   // at once on a change, else every 2 s
    n2k.lastBank = bank;
    n2k.tSwitch = now;
    SetN2kPGN127501(m, cfg.n2kBankInst, bank);
    n2kSend(m);
  }
  if (now - n2k.tBatt >= 1500 && s.vin.valid) {
    n2k.tBatt = now;
    SetN2kDCBatStatus(m, cfg.n2kBattInst, s.vin.volts, N2kDoubleNA, N2kDoubleNA, n2k.sid);
    n2kSend(m);
  }
  if (now - n2k.tEnv >= 2000 && baro.ok) {
    n2k.tEnv = now;
    n2k.sid = (n2k.sid + 1) % 253;
    double pa = baro.hPa * 100.0, kelvin = baro.degC + 273.15;
    SetN2kPGN130310(m, n2k.sid, N2kDoubleNA, N2kDoubleNA, pa);
    n2kSend(m);
    SetN2kPGN130314(m, n2k.sid, 0, N2kps_Atmospheric, pa);
    n2kSend(m);
    SetN2kPGN130316(m, n2k.sid, cfg.n2kTempInst, (tN2kTempSource)cfg.n2kTempSrc, kelvin);
    n2kSend(m);
  }
  if (cfg.n2kAlerts) n2kAlerts(s, now);
}
