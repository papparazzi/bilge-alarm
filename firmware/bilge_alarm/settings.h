// Owner settings, stored in flash (NVS namespace "cfg") and edited on the web page.
#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct Settings {
  char name[4][24];         // IN1, IN2, IN3, IN4 (float)
  uint16_t floatDelayS;     // float must stay up this long before the high-water alarm
  uint16_t longRunMin;      // a pump running this long without stopping is an alarm; 0 = off
  uint16_t cyclesPerHour;   // a pump starting this often within an hour is an alarm; 0 = off
  float lowVolt;            // supply below this for a minute is an alarm; 0 = off
  bool lightOnPump;         // light is on while any pump runs
  bool buzzLongRun;         // buzzer beeps for the long-run alarm
  bool buzzCycles;          // buzzer beeps for the cycling alarm
  bool buzzLowVolt;         // buzzer beeps for the low-voltage alarm
  bool bleEnable;           // Bluetooth advertisement for the Cerbo GX
  bool n2kEnable;           // send on NMEA 2000 (takes effect after a restart)
  bool n2kAlerts;           // send alarms as NMEA 2000 alerts (PGN 126983 / 126985)
  uint8_t n2kBankInst;      // switch bank instance for PGN 127501
  uint8_t n2kBattInst;      // battery instance for PGN 127508
  uint8_t n2kTempInst;      // temperature instance for PGN 130316
  uint8_t n2kTempSrc;       // temperature source: 2 inside, 3 engine room, 4 main cabin
  float vinCal;             // supply reading is multiplied by this
  char adminPass[32];       // empty = no login for changes and firmware upload
};

static Settings cfg;

static void settingsDefaults() {
  memset(&cfg, 0, sizeof cfg);
  strlcpy(cfg.name[0], "Pump 1", sizeof cfg.name[0]);
  strlcpy(cfg.name[1], "Pump 2", sizeof cfg.name[1]);
  strlcpy(cfg.name[2], "Pump 3", sizeof cfg.name[2]);
  strlcpy(cfg.name[3], "High water", sizeof cfg.name[3]);
  cfg.floatDelayS = 2;
  cfg.longRunMin = 10;
  cfg.cyclesPerHour = 0;
  cfg.lowVolt = 11.5f;
  cfg.lightOnPump = true;
  cfg.buzzLongRun = true;
  cfg.buzzCycles = false;
  cfg.buzzLowVolt = false;
  cfg.bleEnable = true;
  cfg.n2kEnable = true;
  cfg.n2kAlerts = true;
  cfg.n2kBankInst = 20;
  cfg.n2kBattInst = 5;
  cfg.n2kTempInst = 0;
  cfg.n2kTempSrc = 2;
  cfg.vinCal = 1.0f;
}

// Keeps every value inside a sane range, whatever was stored or typed.
static void settingsClamp() {
  for (auto &n : cfg.name) n[sizeof n - 1] = 0;
  cfg.adminPass[sizeof cfg.adminPass - 1] = 0;
  if (cfg.floatDelayS > 60) cfg.floatDelayS = 60;
  if (cfg.longRunMin > 720) cfg.longRunMin = 720;
  if (cfg.cyclesPerHour > 30) cfg.cyclesPerHour = 30;
  if (!(cfg.lowVolt >= 0.0f) || cfg.lowVolt > 14.0f) cfg.lowVolt = 11.5f;
  if (cfg.n2kBankInst > 252) cfg.n2kBankInst = 20;
  if (cfg.n2kBattInst > 252) cfg.n2kBattInst = 5;
  if (cfg.n2kTempInst > 252) cfg.n2kTempInst = 0;
  if (cfg.n2kTempSrc > 15) cfg.n2kTempSrc = 2;
  if (!(cfg.vinCal >= 0.8f && cfg.vinCal <= 1.2f)) cfg.vinCal = 1.0f;
}

static void settingsLoad() {
  settingsDefaults();
  Preferences p;
  if (!p.begin("cfg", true)) return;   // nothing stored yet
  char key[8];
  for (int i = 0; i < 4; i++) {
    snprintf(key, sizeof key, "name%d", i);
    String s = p.getString(key, cfg.name[i]);
    strlcpy(cfg.name[i], s.c_str(), sizeof cfg.name[i]);
  }
  cfg.floatDelayS = p.getUShort("floatDelay", cfg.floatDelayS);
  cfg.longRunMin = p.getUShort("longRun", cfg.longRunMin);
  cfg.cyclesPerHour = p.getUShort("cycles", cfg.cyclesPerHour);
  cfg.lowVolt = p.getFloat("lowVolt", cfg.lowVolt);
  cfg.lightOnPump = p.getBool("lightOnPump", cfg.lightOnPump);
  cfg.buzzLongRun = p.getBool("buzzLong", cfg.buzzLongRun);
  cfg.buzzCycles = p.getBool("buzzCyc", cfg.buzzCycles);
  cfg.buzzLowVolt = p.getBool("buzzLow", cfg.buzzLowVolt);
  cfg.bleEnable = p.getBool("ble", cfg.bleEnable);
  cfg.vinCal = p.getFloat("vinCal", cfg.vinCal);
  cfg.n2kEnable = p.getBool("n2k", cfg.n2kEnable);
  cfg.n2kAlerts = p.getBool("n2kAlerts", cfg.n2kAlerts);
  cfg.n2kBankInst = p.getUChar("n2kBank", cfg.n2kBankInst);
  cfg.n2kBattInst = p.getUChar("n2kBatt", cfg.n2kBattInst);
  cfg.n2kTempInst = p.getUChar("n2kTemp", cfg.n2kTempInst);
  cfg.n2kTempSrc = p.getUChar("n2kTempSrc", cfg.n2kTempSrc);
  String pw = p.getString("adminPass", "");
  strlcpy(cfg.adminPass, pw.c_str(), sizeof cfg.adminPass);
  p.end();
  settingsClamp();
}

static void settingsSave() {
  settingsClamp();
  Preferences p;
  p.begin("cfg", false);
  char key[8];
  for (int i = 0; i < 4; i++) {
    snprintf(key, sizeof key, "name%d", i);
    p.putString(key, cfg.name[i]);
  }
  p.putUShort("floatDelay", cfg.floatDelayS);
  p.putUShort("longRun", cfg.longRunMin);
  p.putUShort("cycles", cfg.cyclesPerHour);
  p.putFloat("lowVolt", cfg.lowVolt);
  p.putBool("lightOnPump", cfg.lightOnPump);
  p.putBool("buzzLong", cfg.buzzLongRun);
  p.putBool("buzzCyc", cfg.buzzCycles);
  p.putBool("buzzLow", cfg.buzzLowVolt);
  p.putBool("ble", cfg.bleEnable);
  p.putFloat("vinCal", cfg.vinCal);
  p.putBool("n2k", cfg.n2kEnable);
  p.putBool("n2kAlerts", cfg.n2kAlerts);
  p.putUChar("n2kBank", cfg.n2kBankInst);
  p.putUChar("n2kBatt", cfg.n2kBattInst);
  p.putUChar("n2kTemp", cfg.n2kTempInst);
  p.putUChar("n2kTempSrc", cfg.n2kTempSrc);
  p.putString("adminPass", cfg.adminPass);
  p.end();
}
