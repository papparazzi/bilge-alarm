// Outputs, LEDs, the MUTE button and the I2C sensor.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "console.h"
#include "ms5637.h"

static struct {
  bool light, buzzer;       // 12 V high-side outputs
  bool ledStat, ledAlm;
  bool ledAuto = true;      // LEDs follow Wi-Fi state / high-water input until overridden
  bool lampTest;            // MUTE held: both 12 V outputs on
} out;

static struct {
  bool pressed;
  uint32_t since;
  uint32_t presses;
} mute;

static struct {
  bool ok;
  float hPa, degC;
  uint32_t fails;
} baro;

static Ms5637 ms5637;
static String i2cFound;   // "0x76 0x44" after a scan

static void applyOutputs() {
  digitalWrite(PIN_LIGHT, out.light || out.lampTest);
  digitalWrite(PIN_BUZZ, out.buzzer || out.lampTest);
  digitalWrite(PIN_LED_STAT, out.ledStat);
  digitalWrite(PIN_LED_ALM, out.ledAlm);
}

static void i2cScan() {
  i2cFound = "";
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char b[8];
      snprintf(b, sizeof b, "0x%02X ", a);
      i2cFound += b;
    }
  }
  i2cFound.trim();
  if (!i2cFound.length()) i2cFound = "none";
}

static void boardBegin() {
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_LED_STAT, OUTPUT);
  pinMode(PIN_LED_ALM, OUTPUT);
  applyOutputs();
  pinMode(PIN_BTN_MUTE, INPUT_PULLUP);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  Wire.setTimeOut(50);
  i2cScan();
  if (!ms5637.begin(Wire)) con.println("MS5637: no answer at 0x76");
  else if (!ms5637.crcOk) con.println("MS5637: calibration PROM CRC mismatch");
}

static void baroRead() {
  if (!ms5637.present && !ms5637.begin(Wire)) {
    baro.ok = false;
    return;
  }
  baro.ok = ms5637.read(baro.hPa, baro.degC);
  if (!baro.ok) {
    baro.fails++;
    ms5637.present = false;   // re-probe next time
  }
}

// Short press: counted. Held: lamp and buzzer test for as long as it is held.
static void muteLoop() {
  static uint32_t lastChange = 0;
  static bool raw = false;
  bool now = digitalRead(PIN_BTN_MUTE) == LOW;
  if (now != raw) {
    raw = now;
    lastChange = millis();
  }
  if (millis() - lastChange < 30 || raw == mute.pressed) {
    if (mute.pressed && !out.lampTest && millis() - mute.since >= MUTE_LONG_PRESS_MS) {
      out.lampTest = true;
      applyOutputs();
    }
    return;
  }
  mute.pressed = raw;
  if (mute.pressed) {
    mute.since = millis();
    mute.presses++;
  } else if (out.lampTest) {
    out.lampTest = false;
    applyOutputs();
  }
}
