// The alarm core: reads the inputs, decides the alarms, drives the light, buzzer and LEDs,
// and keeps the counters and the event log.
//
// It runs in its own task, above the priority of the web server, Bluetooth and everything
// else, so nothing on the network side can delay the buzzer. Other code reads its state
// only through coreSnapshot() / coreEvents() and changes it only through the core*() calls.
//
// Counters and the event log live in RTC memory: they survive a restart, a crash and a
// firmware update, and start from zero only when the board loses power.
#pragma once
#include <Arduino.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_task_wdt.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "config.h"
#include "console.h"
#include "settings.h"

#define NCH      4     // IN1..IN4
#define NPUMP    3     // IN1..IN3 are pumps
#define CH_FLOAT 3     // IN4 is the high-water float
#define EV_MAX   80    // events kept
#define CYC_MAX  32    // pump starts remembered for the cycling alarm

enum AlarmId : uint8_t { AL_HIGH_WATER, AL_LONG1, AL_LONG2, AL_LONG3, AL_CYC1, AL_CYC2, AL_CYC3, AL_LOW_VOLT, AL_N };
enum EvType : uint8_t { EV_BOOT, EV_INPUT_ON, EV_INPUT_OFF, EV_ALARM_ON, EV_ALARM_OFF, EV_MUTE, EV_COUNTERS_RESET, EV_WIFI_RESET };

struct ChanStat {      // since power-on
  uint32_t count;      // times the input switched on
  uint32_t totalS;     // total time on, finished runs only
  uint32_t lastDurS;   // length of the last finished run
  uint32_t longestS;
  uint32_t lastOnS;    // when it last switched on (seconds since power-on)
};
struct AlarmStat {
  uint32_t count;
  uint32_t lastOnS;
};
struct Event {
  uint32_t t;          // seconds since power-on
  uint8_t type;        // EvType
  uint8_t idx;         // input or alarm number
  uint16_t val;        // run length in seconds, or volts x 100, depending on type
};
struct Persist {
  uint32_t magic;
  uint32_t upS;        // seconds since power-on, carried across restarts
  uint32_t restarts;   // restarts since power-on
  uint16_t bleSeq;
  uint16_t evHead;     // next slot to write
  uint16_t evCount;
  uint16_t pad;
  ChanStat ch[NCH];
  AlarmStat al[AL_N];
  Event ev[EV_MAX];
  uint32_t crc;
};
#define PERSIST_MAGIC 0xB116E001UL   // change when the layout of Persist changes

struct ChanLive {
  float volts;         // at the terminal
  int mv;              // at the ESP32 pin
  bool valid;          // the last reading produced a value
  bool on;
  uint8_t deb;
  uint32_t onSinceS;
  uint32_t adcReads, adcErr;
  uint32_t cyc[CYC_MAX];   // start times, newest at cycHead - 1
  uint8_t cycHead, cycN;
  uint8_t startsLastHour;
};
struct AlarmLive {
  bool active, muted;
  uint32_t sinceS;
};
#define VIN_MEDIAN 5   // readings in the supply-voltage median filter
struct VinState {
  float volts;         // filtered
  int mv;              // median at the pin
  bool valid;
  float minV, maxV;    // since this start; 0 = nothing yet
  int hist[VIN_MEDIAN];
  uint8_t histN, histI;
  uint32_t lowSinceMs, okSinceMs;
};
struct OutState {
  bool light, buzzer, ledStat, ledAlm, lampTest;
  uint32_t testLightUntil, testBuzzUntil;
  uint32_t chirpUntil;   // three short beeps: Wi-Fi reset accepted
};
struct MuteState {
  bool pressed, rawLast;
  bool wifiDone;         // this press already triggered the Wi-Fi reset
  uint8_t stable;
  uint32_t sinceMs, presses;
};
struct Snapshot {
  uint32_t nowS;       // seconds since power-on
  uint32_t restarts;
  bool unacked;        // an alarm happened that nobody has acknowledged yet
  ChanLive ch[NCH];
  ChanStat cs[NCH];
  AlarmLive al[AL_N];
  AlarmStat as[AL_N];
  VinState vin;
  OutState out;
  MuteState mute;
};

RTC_NOINIT_ATTR static Persist P;
static ChanLive chLive[NCH];
static AlarmLive alLive[AL_N];
static VinState vin;
static OutState out;
static MuteState mute;
static bool unacked;
static bool persistFresh;            // counters started from zero at this boot
static uint32_t upBaseS;
static SemaphoreHandle_t coreMux;
static volatile uint8_t netLed = 0;  // 0 joining, 1 connected, 2 setup hotspot (set by net.h)
static volatile bool coreWifiResetReq;   // MUTE was held long enough; the main loop does the reset

static const char *const ALARM_KEY[AL_N] = {"high_water", "long_run_1", "long_run_2", "long_run_3", "cycling_1", "cycling_2", "cycling_3", "low_voltage"};

// ---------------------------------------------------------------- time and persistence
static uint32_t upS() { return upBaseS + (uint32_t)(esp_timer_get_time() / 1000000LL); }

static void persistSeal() { P.crc = esp_rom_crc32_le(0, (const uint8_t *)&P, offsetof(Persist, crc)); }

static void logEvent(uint8_t type, uint8_t idx, uint32_t val) {
  Event &e = P.ev[P.evHead];
  e.t = upS();
  e.type = type;
  e.idx = idx;
  e.val = val > 65535 ? 65535 : (uint16_t)val;
  P.evHead = (P.evHead + 1) % EV_MAX;
  if (P.evCount < EV_MAX) P.evCount++;
}

static void persistBegin() {
  bool ok = P.magic == PERSIST_MAGIC && P.crc == esp_rom_crc32_le(0, (const uint8_t *)&P, offsetof(Persist, crc)) && P.evHead < EV_MAX && P.evCount <= EV_MAX;
  if (!ok) {
    memset(&P, 0, sizeof P);
    P.magic = PERSIST_MAGIC;
    P.bleSeq = (uint16_t)esp_random();
    persistFresh = true;
  } else {
    P.restarts++;
  }
  upBaseS = P.upS;
  logEvent(EV_BOOT, (uint8_t)esp_reset_reason(), P.restarts);
  persistSeal();
}

// ---------------------------------------------------------------- ADC
struct AdcCh {
  int pin;
  adc_unit_t unit;
  adc_channel_t chan;
  adc_cali_handle_t cali;
  bool ready;
};
static AdcCh adcCh[NCH + 1] = {{PIN_IN1}, {PIN_IN2}, {PIN_IN3}, {PIN_IN4}, {PIN_VSENSE}};
static adc_oneshot_unit_handle_t adcUnit[2] = {nullptr, nullptr};

static void adcBegin() {
  for (auto &a : adcCh) {
    if (adc_oneshot_io_to_channel(a.pin, &a.unit, &a.chan) != ESP_OK) continue;
    int u = a.unit == ADC_UNIT_1 ? 0 : 1;
    if (!adcUnit[u]) {
      adc_oneshot_unit_init_cfg_t uc = {};
      uc.unit_id = a.unit;
      uc.ulp_mode = ADC_ULP_MODE_DISABLE;
      if (adc_oneshot_new_unit(&uc, &adcUnit[u]) != ESP_OK) continue;
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

// IN1 and IN2 are on ADC2, which Wi-Fi also uses: single conversions can fail.
// A reading is valid when at least one of its conversions worked.
static bool adcRead(int i, int &mv, uint8_t &errs) {
  AdcCh &a = adcCh[i];
  errs = 0;
  if (!a.ready) {
    errs = ADC_SAMPLES;
    return false;
  }
  int u = a.unit == ADC_UNIT_1 ? 0 : 1;
  long sum = 0;
  int n = 0;
  for (int k = 0; k < ADC_SAMPLES; k++) {
    int raw = 0;
    if (adc_oneshot_read(adcUnit[u], a.chan, &raw) == ESP_OK) {
      sum += raw;
      n++;
    } else {
      errs++;
    }
  }
  if (!n) return false;
  int raw = sum / n;
  if (!a.cali || adc_cali_raw_to_voltage(a.cali, raw, &mv) != ESP_OK) mv = raw * 3100 / 4095;
  return true;
}

// ---------------------------------------------------------------- inputs and alarms (call with coreMux held)
static uint8_t startsInLastHour(const ChanLive &L, uint32_t now) {
  uint8_t n = 0;
  for (uint8_t k = 0; k < L.cycN; k++) {
    if (now - L.cyc[k] < 3600) n++;
  }
  return n;
}

static void setInput(int c, bool on, uint32_t now) {
  ChanLive &L = chLive[c];
  ChanStat &S = P.ch[c];
  L.on = on;
  if (on) {
    L.onSinceS = now;
    S.count++;
    S.lastOnS = now;
    L.cyc[L.cycHead] = now;
    L.cycHead = (L.cycHead + 1) % CYC_MAX;
    if (L.cycN < CYC_MAX) L.cycN++;
    logEvent(EV_INPUT_ON, c, (uint32_t)(L.volts * 100));
  } else {
    uint32_t dur = now - L.onSinceS;
    S.totalS += dur;
    S.lastDurS = dur;
    if (dur > S.longestS) S.longestS = dur;
    logEvent(EV_INPUT_OFF, c, dur);
  }
}

static void setAlarm(uint8_t id, bool active, uint32_t now) {
  AlarmLive &A = alLive[id];
  if (A.active == active) return;
  A.active = active;
  if (active) {
    A.muted = false;
    A.sinceS = now;
    P.al[id].count++;
    P.al[id].lastOnS = now;
    unacked = true;
    logEvent(EV_ALARM_ON, id, 0);
  } else {
    logEvent(EV_ALARM_OFF, id, now - A.sinceS);
  }
}

static void evalAlarms(uint32_t now, uint32_t nowMs) {
  ChanLive &F = chLive[CH_FLOAT];
  setAlarm(AL_HIGH_WATER, F.on && now - F.onSinceS >= cfg.floatDelayS, now);
  for (int p = 0; p < NPUMP; p++) {
    ChanLive &L = chLive[p];
    setAlarm(AL_LONG1 + p, cfg.longRunMin && L.on && now - L.onSinceS >= (uint32_t)cfg.longRunMin * 60, now);
    L.startsLastHour = startsInLastHour(L, now);
    setAlarm(AL_CYC1 + p, cfg.cyclesPerHour && L.startsLastHour >= cfg.cyclesPerHour, now);
  }
  // Supply voltage: slow to raise, slow to clear, and ignored on USB power.
  bool watch = cfg.lowVolt > 0 && vin.valid && vin.volts > USB_ONLY_VOLTS;
  if (!watch) {
    vin.lowSinceMs = vin.okSinceMs = 0;
    setAlarm(AL_LOW_VOLT, false, now);
  } else if (vin.volts < cfg.lowVolt) {
    vin.okSinceMs = 0;
    if (!vin.lowSinceMs) vin.lowSinceMs = nowMs | 1;   // "| 1" keeps 0 free to mean "not timing"
    if ((int32_t)(nowMs - vin.lowSinceMs) >= (int32_t)LOW_VOLT_DELAY_MS) setAlarm(AL_LOW_VOLT, true, now);   // signed: the stamp can be 1 ms ahead
  } else if (vin.volts > cfg.lowVolt + 0.3f) {
    vin.lowSinceMs = 0;
    if (!vin.okSinceMs) vin.okSinceMs = nowMs | 1;
    if ((int32_t)(nowMs - vin.okSinceMs) >= (int32_t)LOW_VOLT_CLEAR_MS) setAlarm(AL_LOW_VOLT, false, now);
  }
}

// Silences every alarm that is active now. A new alarm sounds again.
// The hard-wired bypass still holds light and buzzer on while the float is up.
static bool muteAllLocked() {
  bool any = unacked;
  for (auto &A : alLive) {
    if (A.active && !A.muted) {
      A.muted = true;
      any = true;
    }
  }
  unacked = false;
  if (any) logEvent(EV_MUTE, 0, 0);
  return any;
}

static bool muteButton(uint32_t nowMs) {
  bool changed = false;
  bool raw = digitalRead(PIN_BTN_MUTE) == LOW;
  if (raw == mute.rawLast) {
    if (mute.stable < 255) mute.stable++;
  } else {
    mute.rawLast = raw;
    mute.stable = 0;
  }
  if (mute.stable >= 1 && raw != mute.pressed) {
    mute.pressed = raw;
    if (raw) {
      mute.sinceMs = nowMs;
      mute.presses++;
      changed = muteAllLocked();
    } else {
      out.lampTest = false;
      mute.wifiDone = false;
    }
  }
  // Held: lamp and buzzer test. Held much longer: Wi-Fi reset, confirmed by three beeps.
  if (mute.pressed && !mute.wifiDone) {
    uint32_t held = nowMs - mute.sinceMs;
    if (held >= MUTE_WIFI_RESET_MS) {
      mute.wifiDone = true;
      out.lampTest = false;
      out.chirpUntil = (nowMs + 750) | 1;
      coreWifiResetReq = true;
      logEvent(EV_WIFI_RESET, 0, 0);
      changed = true;
    } else if (held >= MUTE_LONG_PRESS_MS) {
      out.lampTest = true;
    }
  }
  return changed;
}

static void driveOutputs(uint32_t nowMs) {
  bool any = false, beep = false, pump = false;
  for (auto &A : alLive) any |= A.active;
  for (int p = 0; p < NPUMP; p++) {
    pump |= chLive[p].on;
    beep |= cfg.buzzLongRun && alLive[AL_LONG1 + p].active && !alLive[AL_LONG1 + p].muted;
    beep |= cfg.buzzCycles && alLive[AL_CYC1 + p].active && !alLive[AL_CYC1 + p].muted;
  }
  beep |= cfg.buzzLowVolt && alLive[AL_LOW_VOLT].active && !alLive[AL_LOW_VOLT].muted;
  bool steady = alLive[AL_HIGH_WATER].active && !alLive[AL_HIGH_WATER].muted;
  bool testL = out.testLightUntil && (int32_t)(out.testLightUntil - nowMs) > 0;
  bool testB = out.testBuzzUntil && (int32_t)(out.testBuzzUntil - nowMs) > 0;
  if (!testL) out.testLightUntil = 0;
  if (!testB) out.testBuzzUntil = 0;
  bool chirp = out.chirpUntil && (int32_t)(out.chirpUntil - nowMs) > 0;
  if (!chirp) out.chirpUntil = 0;

  out.light = out.lampTest || testL || any || (cfg.lightOnPump && pump);
  out.buzzer = out.lampTest || testB || steady || (beep && nowMs % 3000 < 300) || (chirp && (out.chirpUntil - nowMs) % 250 >= 150);
  out.ledAlm = any || (unacked && nowMs % 1000 < 150);   // solid: alarm now; blinking: there was one
  out.ledStat = netLed == 1 ? nowMs % 2000 < 60          // short flash every 2 s: on the network
              : netLed == 2 ? nowMs % 1000 < 500         // slow even blink: setup hotspot
              : nowMs % 200 < 100;                       // fast blink: joining
  digitalWrite(PIN_LIGHT, out.light);
  digitalWrite(PIN_BUZZ, out.buzzer);
  digitalWrite(PIN_LED_STAT, out.ledStat);
  digitalWrite(PIN_LED_ALM, out.ledAlm);
}

// ---------------------------------------------------------------- the task
static void coreTick() {
  int mv[NCH + 1] = {0};
  bool ok[NCH + 1];
  uint8_t errs[NCH + 1];
  for (int i = 0; i <= NCH; i++) ok[i] = adcRead(i, mv[i], errs[i]);   // outside the lock: this is the slow part

  uint32_t nowMs = millis(), now = upS();
  xSemaphoreTake(coreMux, portMAX_DELAY);
  bool changed = false;
  for (int c = 0; c < NCH; c++) {
    ChanLive &L = chLive[c];
    L.adcReads += ADC_SAMPLES;
    L.adcErr += errs[c];
    L.valid = ok[c];
    if (!ok[c]) continue;   // keep the last state: a failed read is not "pump off"
    L.mv = mv[c];
    L.volts = mv[c] / 1000.0f * DIVIDER_RATIO;
    bool want = L.on ? L.volts >= IN_OFF_VOLTS : L.volts >= IN_ON_VOLTS;
    if (want == L.on) {
      L.deb = 0;
    } else if (++L.deb >= DEBOUNCE_TICKS) {
      L.deb = 0;
      setInput(c, want, now);
      changed = true;
    }
  }
  // Supply voltage: single readings jump up by as much as 1 V now and then (seen on the bench
  // with Wi-Fi on), so take the median of the last few and smooth it. The supply moves slowly.
  vin.valid = ok[NCH];
  if (ok[NCH]) {
    vin.hist[vin.histI] = mv[NCH];
    vin.histI = (vin.histI + 1) % VIN_MEDIAN;
    if (vin.histN < VIN_MEDIAN) vin.histN++;
    int sorted[VIN_MEDIAN];
    for (int i = 0; i < vin.histN; i++) {
      int v = vin.hist[i], k = i;
      for (; k > 0 && sorted[k - 1] > v; k--) sorted[k] = sorted[k - 1];
      sorted[k] = v;
    }
    vin.mv = sorted[vin.histN / 2];
    float v = vin.mv / 1000.0f * DIVIDER_RATIO * cfg.vinCal;
    if (vin.histN < VIN_MEDIAN) {
      vin.volts = v;
    } else {
      vin.volts += 0.2f * (v - vin.volts);
      if (nowMs > 3000 && vin.volts > USB_ONLY_VOLTS) {
        if (vin.minV == 0 || vin.volts < vin.minV) vin.minV = vin.volts;
        if (vin.volts > vin.maxV) vin.maxV = vin.volts;
      }
    }
  }
  uint16_t evBefore = P.evHead;
  evalAlarms(now, nowMs);
  changed |= muteButton(nowMs);
  changed |= evBefore != P.evHead;
  driveOutputs(nowMs);
  if (now != P.upS) {
    P.upS = now;
    changed = true;
  }
  if (changed) persistSeal();
  xSemaphoreGive(coreMux);
}

static void coreTask(void *) {
  esp_task_wdt_add(NULL);   // a stuck alarm task restarts the board
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    coreTick();
    esp_task_wdt_reset();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(CORE_TICK_MS));
  }
}

static void coreBegin() {
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_LED_STAT, OUTPUT);
  pinMode(PIN_LED_ALM, OUTPUT);
  digitalWrite(PIN_LIGHT, LOW);
  digitalWrite(PIN_BUZZ, LOW);
  pinMode(PIN_BTN_MUTE, INPUT_PULLUP);
  coreMux = xSemaphoreCreateMutex();
  persistBegin();
  adcBegin();
  xTaskCreatePinnedToCore(coreTask, "alarm", 4096, nullptr, 5, nullptr, 1);
}

// ---------------------------------------------------------------- access from other tasks
static void coreSnapshot(Snapshot &s) {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  s.nowS = upS();
  s.restarts = P.restarts;
  s.unacked = unacked;
  memcpy(s.ch, chLive, sizeof chLive);
  memcpy(s.cs, P.ch, sizeof P.ch);
  memcpy(s.al, alLive, sizeof alLive);
  memcpy(s.as, P.al, sizeof P.al);
  s.vin = vin;
  s.out = out;
  s.mute = mute;
  xSemaphoreGive(coreMux);
}

// Copies up to max events, newest first. Returns how many.
static int coreEvents(Event *dst, int max) {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  int n = P.evCount < max ? P.evCount : max;
  for (int k = 0; k < n; k++) dst[k] = P.ev[(P.evHead + EV_MAX - 1 - k) % EV_MAX];
  xSemaphoreGive(coreMux);
  return n;
}

static void coreMute() {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  if (muteAllLocked()) persistSeal();
  xSemaphoreGive(coreMux);
}

static void coreTestOutput(bool light, bool buzzer) {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  uint32_t until = (millis() + OUTPUT_TEST_MS) | 1;
  if (light) out.testLightUntil = until;
  if (buzzer) out.testBuzzUntil = until;
  xSemaphoreGive(coreMux);
}

// Zeroes the counters and the log. Inputs that are on right now count as one fresh start.
static void coreResetCounters() {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  uint32_t now = upS();
  memset(P.ch, 0, sizeof P.ch);
  memset(P.al, 0, sizeof P.al);
  P.evHead = P.evCount = 0;
  for (int c = 0; c < NCH; c++) {
    chLive[c].cycN = chLive[c].cycHead = 0;
    if (chLive[c].on) {
      P.ch[c].count = 1;
      P.ch[c].lastOnS = chLive[c].onSinceS;
    }
  }
  for (int a = 0; a < AL_N; a++) {
    if (alLive[a].active) {
      P.al[a].count = 1;
      P.al[a].lastOnS = alLive[a].sinceS;
    }
  }
  (void)now;
  logEvent(EV_COUNTERS_RESET, 0, 0);
  persistSeal();
  xSemaphoreGive(coreMux);
}

// Settings are read by the alarm task, so they change under the same lock.
static void coreApplySettings(const Settings &s) {
  xSemaphoreTake(coreMux, portMAX_DELAY);
  cfg = s;
  settingsClamp();
  xSemaphoreGive(coreMux);
}

// Text for an alarm, using the owner's input names.
static String alarmText(uint8_t id) {
  if (id == AL_HIGH_WATER) return String(cfg.name[CH_FLOAT]);
  if (id >= AL_LONG1 && id <= AL_LONG3) return String(cfg.name[id - AL_LONG1]) + " running too long";
  if (id >= AL_CYC1 && id <= AL_CYC3) return String(cfg.name[id - AL_CYC1]) + " starting too often";
  return "Supply voltage low";
}
