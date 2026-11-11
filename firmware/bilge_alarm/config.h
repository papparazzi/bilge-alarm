// Bilge alarm Rev C.5 - alarm firmware: pins and fixed constants.
// Pin map source: docs/DEVBOARD_PINMAP_revC.md (ESP32-S3-DevKitC-1 clone on sockets).
// Everything an owner may want to change at run time is in settings.h, not here.
#pragma once

#define FW_NAME    "bilge-alarm"
#define FW_VERSION "1.0.0-rc1"

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
#define DIVIDER_RATIO  (127.0f / 27.0f)
#define IN_ON_VOLTS    8.0f   // input counts as active at or above this
#define IN_OFF_VOLTS   6.0f   // and as inactive again below this
#define ADC_SAMPLES    4      // conversions averaged per reading
#define DEBOUNCE_TICKS 3      // readings in a row before an input changes state

// ---- alarm core
#define CORE_TICK_MS       50      // inputs are read and outputs driven this often
#define MUTE_LONG_PRESS_MS 1500UL  // hold = lamp and buzzer test
#define MUTE_WIFI_RESET_MS 10000UL // keep holding = forget the Wi-Fi network and open the setup hotspot
#define LOW_VOLT_DELAY_MS  60000UL // supply must stay low this long before the alarm
#define LOW_VOLT_CLEAR_MS  10000UL // and stay 0.3 V above the limit this long to clear it
#define USB_ONLY_VOLTS     5.0f    // below this the board is on USB power: no low-voltage alarm
#define OUTPUT_TEST_MS     5000UL  // length of a web-page output test

// ---- NMEA 2000
#define CAN_BITRATE_KBPS 250

// ---- Wi-Fi
#define HOSTNAME               "bilge-alarm"   // http://bilge-alarm.local
#define AP_SSID_PREFIX         "BilgeAlarm-"   // + last 4 hex digits of the MAC
#include "secrets.h"                           // AP_PASSWORD, the setup hotspot password. Missing? Copy secrets.example.h to secrets.h
#define STA_CONNECT_TIMEOUT_MS 20000UL         // no connection after this: open the setup hotspot
#define STA_LOST_TO_AP_MS      60000UL         // connection lost for this long: open the setup hotspot
#define AP_RETRY_STA_MS        300000UL        // hotspot idle this long with a saved network: try it again

// ---- Bluetooth (RuuviTag format 5 advertisement for the Victron Cerbo GX)
#define BLE_UPDATE_MS 2500UL
