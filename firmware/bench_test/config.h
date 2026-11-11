// Bilge alarm Rev C.5 - bench test firmware: pins and constants.
// Pin map source: docs/DEVBOARD_PINMAP_revC.md (ESP32-S3-DevKitC-1 clone on sockets).
#pragma once

#define FW_NAME    "bilge-alarm bench test"
#define FW_VERSION "0.1.0"

// ---- GPIO (dev board J1 row unless noted)
#define PIN_CAN_RX    4    // CA-IS3050G RXD
#define PIN_CAN_TX    5    // CA-IS3050G TXD (10k pull-up on the PCB)
#define PIN_VSENSE    6    // ADC1: supply voltage, 100k/27k
#define PIN_BTN_MUTE  8    // active low, 10k pull-up on the PCB
#define PIN_IN4       9    // ADC1: high-water float
#define PIN_IN3       10   // ADC1: pump 3 run
#define PIN_IN2       11   // ADC2: pump 2 run (ADC2 shares hardware with Wi-Fi)
#define PIN_IN1       12   // ADC2: pump 1 run (ADC2 shares hardware with Wi-Fi)
#define PIN_LED_STAT  13   // active high, 470R
#define PIN_LED_ALM   14   // active high, 470R
#define PIN_LIGHT     16   // active high -> Q2 -> high-side Q6 -> LGT+
#define PIN_BUZZ      17   // active high -> Q3 -> high-side Q7 -> BUZ+
#define PIN_I2C_SCL   41   // dev board J3 row
#define PIN_I2C_SDA   42   // dev board J3 row

// ---- analogue front end: 100k (top) / 27k (bottom) on IN1-4 and VSENSE
#define DIVIDER_RATIO (127.0f / 27.0f)
#define IN_ON_VOLTS   8.0f   // input counts as active at or above this
#define IN_OFF_VOLTS  6.0f   // and as inactive again below this

// ---- NMEA 2000
#define CAN_BITRATE_KBPS 250

// ---- Wi-Fi
#define HOSTNAME               "bilge-alarm"   // http://bilge-alarm.local
#define AP_SSID_PREFIX         "BilgeAlarm-"   // + last 4 hex digits of the MAC
#include "secrets.h"                           // AP_PASSWORD, the setup hotspot password. Missing? Copy secrets.example.h to secrets.h
#define STA_CONNECT_TIMEOUT_MS 20000UL         // no connection after this: start the setup hotspot
#define STA_LOST_REBOOT_MS     60000UL         // connection lost for this long: reboot (then hotspot if it still fails)
#define AP_RETRY_STA_MS        300000UL        // hotspot idle this long with a saved network: reboot and try it again

// ---- MUTE button
#define MUTE_LONG_PRESS_MS 1500UL   // hold = lamp and buzzer test
