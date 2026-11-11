// Bluetooth for the Victron Cerbo GX: the board advertises as a RuuviTag (data format 5),
// which Venus OS shows as a temperature sensor with pressure, with nothing installed on the Cerbo.
// Layout: https://docs.ruuvi.com/communication/bluetooth-advertisements/data-format-5-rawv2
//
// It carries temperature and pressure only. Humidity, acceleration and movement are sent as
// "not available". The battery field cannot hold a 12 V supply (it tops out at 3.6 V), so it
// carries the board's nominal 3.3 V logic supply to keep the Cerbo's low-battery alarm quiet.
//
// Advertising pauses while someone is connected to the setup hotspot: Espressif rates
// Bluetooth next to an active hotspot as unstable.
#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include "config.h"
#include "console.h"
#include "core.h"
#include "board.h"
#include "net.h"

static struct {
  bool started;      // Bluetooth stack is up
  bool advertising;
  bool failed;
  uint32_t frames;   // advertisement updates since this boot
  uint32_t lastMs;
  String mac;
} ble;

static void bleBuildFrame(uint8_t *ad) {
  uint8_t *p = ad + 4;
  ad[0] = 27;     // length of what follows
  ad[1] = 0xFF;   // manufacturer specific data
  ad[2] = 0x99;   // Ruuvi Innovations, 0x0499, little-endian
  ad[3] = 0x04;
  int16_t t = baro.ok ? (int16_t)lroundf(baro.degC / 0.005f) : (int16_t)0x8000;
  uint16_t pr = 0xFFFF;
  if (baro.ok) {
    long pa = lroundf(baro.hPa * 100.0f) - 50000;
    if (pa >= 0 && pa < 65535) pr = (uint16_t)pa;
  }
  uint16_t power = ((3300 - 1600) << 5) | 22;   // 3.3 V, +4 dBm
  xSemaphoreTake(coreMux, portMAX_DELAY);
  uint16_t seq = ++P.bleSeq;   // kept across restarts: the Cerbo drops a number that runs backwards
  persistSeal();
  xSemaphoreGive(coreMux);
  p[0] = 0x05;
  p[1] = t >> 8;
  p[2] = t & 0xFF;
  p[3] = 0xFF;   // humidity: not available
  p[4] = 0xFF;
  p[5] = pr >> 8;
  p[6] = pr & 0xFF;
  for (int k = 7; k < 13; k += 2) {   // acceleration x, y, z: not available
    p[k] = 0x80;
    p[k + 1] = 0x00;
  }
  p[13] = power >> 8;
  p[14] = power & 0xFF;
  p[15] = 0xFF;   // movement counter: not available
  p[16] = seq >> 8;
  p[17] = seq & 0xFF;
  esp_read_mac(p + 18, ESP_MAC_BT);
}

static void bleStop() {
  if (!ble.advertising) return;
  BLEDevice::getAdvertising()->stop();
  ble.advertising = false;
}

static void bleLoop() {
  bool want = cfg.bleEnable && !ble.failed && !(netState == NET_AP && WiFi.softAPgetStationNum() > 0);
  if (!want) {
    bleStop();
    return;
  }
  uint32_t now = millis();
  if (ble.advertising && now - ble.lastMs < BLE_UPDATE_MS) return;
  if (!ble.started) {
    if (!BLEDevice::init("")) {
      ble.failed = true;
      con.println("BLE: could not start");
      return;
    }
    ble.started = true;
    ble.mac = BLEDevice::getAddress().toString().c_str();
    con.printf("BLE: advertising as a RuuviTag from %s\n", ble.mac.c_str());
  }
  uint8_t ad[28];
  bleBuildFrame(ad);
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  if (ble.advertising) adv->stop();
  BLEAdvertisementData data;
  data.setFlags(0x06);   // general discoverable, no classic Bluetooth
  data.addData((char *)ad, sizeof ad);
  adv->setAdvertisementData(data);
  adv->setScanResponse(false);
#if defined(CONFIG_NIMBLE_ENABLED)
  adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
#else
  adv->setAdvertisementType(ADV_TYPE_NONCONN_IND);
#endif
  adv->setMinInterval(320);   // 200 ms: the Cerbo listens only about a sixth of the time
  adv->setMaxInterval(400);   // 250 ms
  ble.advertising = adv->start();
  ble.lastMs = now;
  if (ble.advertising) ble.frames++;
}
