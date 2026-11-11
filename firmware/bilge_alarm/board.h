// The I2C sensor. (Outputs, LEDs, inputs and the MUTE button belong to core.h.)
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "console.h"
#include "ms5637.h"

static struct {
  bool ok;
  float hPa, degC;
  uint32_t fails;
} baro;

static Ms5637 ms5637;
static String i2cFound;   // "0x76 0x44" after a scan

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
