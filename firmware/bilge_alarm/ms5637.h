// TE MS5637-02BA03 barometric pressure + temperature sensor, I2C address 0x76.
// Maths and CRC from the TE datasheet (06/2017) and application note AN520.
#pragma once
#include <Arduino.h>
#include <Wire.h>

class Ms5637 {
public:
  static const uint8_t ADDR = 0x76;
  bool present = false;   // answered on the bus and the PROM was read
  bool crcOk = false;     // factory calibration PROM passed its CRC-4
  uint16_t prom[8] = {0};

  bool begin(TwoWire &w) {
    wire = &w;
    present = crcOk = false;
    if (!cmd(0x1E)) return false;   // reset, reloads the PROM
    delay(20);
    for (uint8_t i = 0; i < 7; i++) {
      if (!cmd(0xA0 + 2 * i)) return false;
      if (wire->requestFrom(ADDR, (uint8_t)2) != 2) return false;
      prom[i] = (uint16_t)wire->read() << 8;
      prom[i] |= wire->read();
    }
    crcOk = crc4() == (prom[0] >> 12);
    present = true;
    return true;
  }

  // Blocks for about 20 ms (two conversions at OSR 4096).
  bool read(float &hPa, float &degC) {
    uint32_t d1, d2;
    if (!convert(0x48, d1) || !convert(0x58, d2)) return false;
    int64_t dT = (int64_t)d2 - ((int64_t)prom[5] << 8);
    int64_t temp = 2000 + ((dT * prom[6]) >> 23);
    int64_t off = ((int64_t)prom[2] << 17) + (((int64_t)prom[4] * dT) >> 6);
    int64_t sens = ((int64_t)prom[1] << 16) + (((int64_t)prom[3] * dT) >> 7);
    int64_t t2, off2 = 0, sens2 = 0;
    if (temp < 2000) {   // second-order compensation, low temperature
      int64_t d = temp - 2000;
      t2 = (3 * dT * dT) >> 33;
      off2 = (61 * d * d) >> 4;
      sens2 = (29 * d * d) >> 4;
      if (temp < -1500) {
        int64_t e = temp + 1500;
        off2 += 17 * e * e;
        sens2 += 9 * e * e;
      }
    } else {
      t2 = (5 * dT * dT) >> 38;
    }
    temp -= t2;
    off -= off2;
    sens -= sens2;
    int64_t p = ((((int64_t)d1 * sens) >> 21) - off) >> 15;   // 0.01 mbar
    hPa = p / 100.0f;
    degC = temp / 100.0f;
    return true;
  }

private:
  TwoWire *wire = nullptr;

  bool cmd(uint8_t c) {
    wire->beginTransmission(ADDR);
    wire->write(c);
    return wire->endTransmission() == 0;
  }

  bool convert(uint8_t c, uint32_t &val) {
    if (!cmd(c)) return false;
    delay(10);   // OSR 4096: 8.22 ms max
    if (!cmd(0x00)) return false;   // ADC read
    if (wire->requestFrom(ADDR, (uint8_t)3) != 3) return false;
    val = (uint32_t)wire->read() << 16;
    val |= (uint32_t)wire->read() << 8;
    val |= wire->read();
    return val != 0;
  }

  uint8_t crc4() {
    uint16_t p[8];
    memcpy(p, prom, sizeof p);
    p[0] &= 0x0FFF;   // the CRC nibble itself counts as zero
    p[7] = 0;
    uint16_t rem = 0;
    for (uint8_t cnt = 0; cnt < 16; cnt++) {
      rem ^= (cnt & 1) ? (p[cnt >> 1] & 0x00FF) : (p[cnt >> 1] >> 8);
      for (uint8_t bit = 8; bit > 0; bit--) rem = (rem & 0x8000) ? (rem << 1) ^ 0x3000 : (rem << 1);
    }
    return (rem >> 12) & 0x0F;
  }
};
