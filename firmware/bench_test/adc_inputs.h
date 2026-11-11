// IN1-IN4 and the supply voltage, read through the 100k/27k dividers.
// Uses the ESP-IDF one-shot ADC driver directly (not analogRead) so that failed
// reads are visible: IN1 and IN2 sit on ADC2, which Wi-Fi also uses.
#pragma once
#include <Arduino.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "config.h"
#include "console.h"

struct AnalogIn {
  const char *name;
  const char *label;
  int pin;
  bool isInput;             // false for the supply voltage channel
  adc_unit_t unit;
  adc_channel_t chan;
  adc_cali_handle_t cali;
  bool ready;               // channel configured
  bool valid;               // last sample set produced a value
  int mv;                   // millivolts at the ESP32 pin
  float volts;              // volts at the terminal
  bool on;                  // above the threshold (with hysteresis)
  uint32_t reads, errors;   // single conversions attempted / failed
  int lastErr;
};

static AnalogIn ain[] = {
  {"IN1", "Pump 1 run", PIN_IN1, true},
  {"IN2", "Pump 2 run", PIN_IN2, true},
  {"IN3", "Pump 3 run", PIN_IN3, true},
  {"IN4", "High-water float", PIN_IN4, true},
  {"VIN", "Supply", PIN_VSENSE, false},
};
static const int AIN_COUNT = sizeof(ain) / sizeof(ain[0]);
#define AIN_IN4 3
#define AIN_VIN 4

static adc_oneshot_unit_handle_t adcUnit[2] = {nullptr, nullptr};

static void adcBegin() {
  for (auto &a : ain) {
    if (adc_oneshot_io_to_channel(a.pin, &a.unit, &a.chan) != ESP_OK) {
      con.printf("ADC: GPIO%d is not an ADC pin\n", a.pin);
      continue;
    }
    int u = a.unit == ADC_UNIT_1 ? 0 : 1;
    if (!adcUnit[u]) {
      adc_oneshot_unit_init_cfg_t uc = {};
      uc.unit_id = a.unit;
      uc.ulp_mode = ADC_ULP_MODE_DISABLE;
      if (adc_oneshot_new_unit(&uc, &adcUnit[u]) != ESP_OK) {
        con.printf("ADC: unit %d init failed\n", u + 1);
        continue;
      }
    }
    adc_oneshot_chan_cfg_t cc = {};
    cc.atten = ADC_ATTEN_DB_12;   // full scale about 3.1 V at the pin = 14.6 V at the terminal
    cc.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_oneshot_config_channel(adcUnit[u], a.chan, &cc) != ESP_OK) continue;
    adc_cali_curve_fitting_config_t cal = {};
    cal.unit_id = a.unit;
    cal.chan = a.chan;
    cal.atten = ADC_ATTEN_DB_12;
    cal.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cal, &a.cali) != ESP_OK) a.cali = nullptr;
    a.ready = true;
  }
}

static void adcSample() {
  for (auto &a : ain) {
    if (!a.ready) continue;
    int u = a.unit == ADC_UNIT_1 ? 0 : 1;
    long sum = 0;
    int n = 0;
    for (int i = 0; i < 8; i++) {
      int raw = 0;
      esp_err_t e = adc_oneshot_read(adcUnit[u], a.chan, &raw);
      a.reads++;
      if (e == ESP_OK) {
        sum += raw;
        n++;
      } else {
        a.errors++;
        a.lastErr = e;
      }
    }
    a.valid = n > 0;
    if (!a.valid) continue;   // keep the last good value and state
    int raw = sum / n, mv = 0;
    if (!a.cali || adc_cali_raw_to_voltage(a.cali, raw, &mv) != ESP_OK) mv = raw * 3100 / 4095;
    a.mv = mv;
    a.volts = mv / 1000.0f * DIVIDER_RATIO;
    if (a.volts >= IN_ON_VOLTS) a.on = true;
    else if (a.volts < IN_OFF_VOLTS) a.on = false;
  }
}
