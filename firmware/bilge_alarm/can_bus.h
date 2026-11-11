// The CAN side of the NMEA 2000 port: the ESP32-S3's built-in controller (TWAI) behind the
// isolated transceiver, 250 kbit/s.
//
// Two home modes:
//   listen-only  NMEA 2000 output is off in settings: the board never transmits and only
//                lists what it hears.
//   normal       n2k.h owns the port and sends through canSend().
// Nothing here ever waits for the bus: a dead, unpowered or missing network cannot hold up
// the rest of the firmware.
#pragma once
#include <Arduino.h>
#include "driver/twai.h"
#include "config.h"
#include "console.h"

struct PgnSeen {
  uint32_t pgn;
  uint8_t src;
  uint32_t count;
};

static struct {
  bool running;
  const char *mode;
  twai_mode_t homeMode = TWAI_MODE_LISTEN_ONLY;
  uint32_t rx;
  uint32_t lastId;
  uint32_t lastRxMs;
  uint32_t txOkMs;      // when a frame of ours was last acknowledged by another device; 0 = never
  uint32_t busOffs;     // times the controller dropped off the bus and was restarted
  uint32_t serviceMs;
  String selfTest = "not run";
  PgnSeen seen[24];
  int nSeen;
} can;

static bool canStart(twai_mode_t mode) {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, mode);
  g.rx_queue_len = 64;
  g.tx_queue_len = 20;
  g.alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_BUS_OFF;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  can.running = false;
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }
  can.running = true;
  can.mode = mode == TWAI_MODE_LISTEN_ONLY ? "listen-only" : mode == TWAI_MODE_NO_ACK ? "self-test" : "normal";
  return true;
}

static void canStop() {
  if (!can.running) return;
  twai_stop();
  twai_driver_uninstall();
  can.running = false;
}

static void canNote(uint32_t id) {
  can.rx++;
  can.lastId = id;
  can.lastRxMs = millis();
  uint8_t pf = (id >> 16) & 0xFF, ps = (id >> 8) & 0xFF, src = id & 0xFF;
  uint32_t pgn = ((id >> 24) & 0x03) << 16 | (uint32_t)pf << 8 | (pf >= 240 ? ps : 0);
  for (int i = 0; i < can.nSeen; i++) {
    if (can.seen[i].pgn == pgn && can.seen[i].src == src) {
      can.seen[i].count++;
      return;
    }
  }
  if (can.nSeen < (int)(sizeof(can.seen) / sizeof(can.seen[0]))) can.seen[can.nSeen++] = {pgn, src, 1};
}

// Next received extended frame, if any. Never waits.
static bool canReceive(uint32_t &id, uint8_t &len, uint8_t *data) {
  twai_message_t m;
  while (can.running && twai_receive(&m, 0) == ESP_OK) {
    if (!m.extd || m.rtr) continue;   // NMEA 2000 uses 29-bit data frames only
    canNote(m.identifier);
    id = m.identifier;
    len = m.data_length_code;
    memcpy(data, m.data, len);
    return true;
  }
  return false;
}

// Queues one frame. Never waits: false when the queue is full or the controller is off the bus.
static bool canSend(uint32_t id, uint8_t len, const uint8_t *data) {
  if (!can.running || len > 8) return false;
  twai_message_t m = {};
  m.extd = 1;
  m.identifier = id;
  m.data_length_code = len;
  memcpy(m.data, data, len);
  return twai_transmit(&m, 0) == ESP_OK;
}

// Listen-only home mode: drain and count what arrives.
static void canPoll() {
  uint32_t id;
  uint8_t len, data[8];
  while (canReceive(id, len, data)) {}
}

// Call often. Notes acknowledged frames and, once a second, restarts a controller that has
// gone off the bus (which is what happens with the bus side unpowered or shorted).
static void canService() {
  if (!can.running) return;
  uint32_t alerts = 0;
  if (twai_read_alerts(&alerts, 0) == ESP_OK && (alerts & TWAI_ALERT_TX_SUCCESS)) can.txOkMs = millis() | 1;
  if (millis() - can.serviceMs < 1000) return;
  can.serviceMs = millis();
  twai_status_info_t st = {};
  if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
    can.busOffs++;
    canStop();
    canStart(can.homeMode);
  }
}

// Sends one proprietary single-frame PGN 65280 from the null address (254) with
// "self reception" on. Needs the bus side powered (12 V on NET-S / NET-C) and a
// termination resistor across CAN-H / CAN-L. Keep it off the boat's live bus.
static String canSelfTest() {
  canStop();
  if (!canStart(TWAI_MODE_NO_ACK)) {
    can.selfTest = "FAIL: CAN driver did not start";
    canStart(can.homeMode);
    return can.selfTest;
  }
  static uint8_t seq = 0;
  twai_message_t tx = {};
  tx.extd = 1;
  tx.self = 1;
  tx.identifier = 0x1CFF00FE;
  tx.data_length_code = 8;
  memcpy(tx.data, "BILGE", 5);
  tx.data[7] = ++seq;
  esp_err_t e = twai_transmit(&tx, pdMS_TO_TICKS(100));
  bool echo = false;
  uint32_t t0 = millis();
  twai_message_t rx;
  while (e == ESP_OK && !echo && millis() - t0 < 400) {
    if (twai_receive(&rx, pdMS_TO_TICKS(50)) != ESP_OK) continue;
    echo = rx.identifier == tx.identifier && memcmp(rx.data, tx.data, 8) == 0;
  }
  twai_status_info_t st = {};
  twai_get_status_info(&st);
  canStop();
  canStart(can.homeMode);
  char buf[160];
  if (echo) {
    snprintf(buf, sizeof buf, "PASS: frame sent and heard back in %lu ms", (unsigned long)(millis() - t0));
  } else {
    snprintf(buf, sizeof buf, "FAIL: no echo (tx error count %lu, bus errors %lu). Is NET-S/NET-C powered and CAN-H/L terminated?",
             (unsigned long)st.tx_error_counter, (unsigned long)st.bus_error_count);
  }
  can.selfTest = buf;
  return can.selfTest;
}
