// ============================================================================
// FieldGuard - MYOSA-based wearable for agricultural worker heat-risk awareness
// Core 1 (loop): 50 Hz sensing, features, activity, risk, OLED, buzzer.
// Core 0 (task): Wi-Fi, MQTT -> Flutter app, local fallback dashboard.
// Indicator-based awareness tool: NOT a diagnosis and NOT a WBGT measurement.
// ============================================================================
#include <Wire.h>
#include "config.h"
#include "types.h"
#include "risk.h"
#include "activity_tracker.h"
#include "signal_processing.h"
#include "classifier.h"
#include "state.h"
#include "sensors.h"
#include "display_alert.h"
#include "connectivity.h"   // needs secrets.h (copy secrets.example.h)

Sensors sensors;
SignalProc sp;
Smoother<ENV_SMOOTH_N> tempSm, pressSm;
ActivityTracker tracker;
RiskEngine riskEng;
Display disp;
Buzzer buzzer;
Connectivity net;
SystemState st;

uint32_t lastImuUs = 0, lastSecMs = 0, lastRetryMs = 0, lastHighBeepMs = 0;
uint8_t imuFail = 0, envFail = 0, pingTick = 0;
const char* faultName = "none";

static uint8_t lightCategory(uint16_t clear) {
  if (clear >= LIGHT_STRONG_CLEAR) return 2;   // includes saturation (max ~4095 @ 10 ms)
  if (clear >= LIGHT_BRIGHT_CLEAR) return 1;
  return 0;
}

static void enterError(const char* what) {
  st.sensorError = true; faultName = what; riskEng = RiskEngine();
}

static void tryInit() {
  if (sensors.begin()) {
    st.sensorError = false; faultName = "none";
    sp.reset(); tempSm.reset(); pressSm.reset(); imuFail = envFail = 0;
    lastImuUs = micros();
  } else {
    faultName = sensors.faultName();
  }
}

static void onWindow() {
  ClassResult r = classifyWindow(sp.interleaved(), sp.rms);
  st.rms = sp.rms; st.var = sp.var; st.usedModel = r.usedModel;
  tracker.onWindow(r.cls);
}

static void onSecond() {
  EnvSample e;
  if (!sensors.readEnv(e)) { if (++envFail >= 5) enterError(sensors.faultName()); return; }
  envFail = 0;
  if ((++pingTick % 5) == 0 && !sensors.pingAll()) { tryInit(); if (!st.sensorError) return; enterError(faultName); return; }

  float t = tempSm.add(e.tempC) * TEMP_CAL_SLOPE + TEMP_CAL_OFFSET_C;
  st.tempC = t;
  st.tempEff = t + (st.demo ? DEMO_TEMP_OFFSET_C : 0.0f);
  st.pressPa = pressSm.add(e.pressPa);
  st.clear = e.clear;
  st.light = lightCategory(e.clear);

  int dt = st.demo ? DEMO_TIME_SCALE : 1;
  for (int i = 0; i < dt; i++) tracker.tick1s();

  st.act = tracker.confirmed;
  st.dact = tracker.dact; st.drest = tracker.restRun; st.rest60 = tracker.rest60();

  RiskInputs in{st.tempEff, st.act, st.dact, st.rest60, st.light};
  st.rb = computeRisk(in);
  st.risk = riskEng.update(st.rb.score);

  uint32_t now = millis();
  if (riskEng.rose()) {
    if (st.risk == RISK_HIGH) buzzer.pattern(3, 200, 150);
    else buzzer.pattern(1, 150, 100);                    // Caution: one short beep
    lastHighBeepMs = now;
  } else if (st.risk == RISK_HIGH && now - lastHighBeepMs >= HIGH_REPEAT_MS) {
    buzzer.pattern(2, 200, 150); lastHighBeepMs = now;   // low-rate repeat (alarm fatigue)
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_CLOCK);
  buzzer.begin();
  disp.begin();
  net.begin();
  tryInit();
  if (!sensors.pingAll()) st.sensorError = true;
  lastImuUs = micros(); lastSecMs = millis();
}

void loop() {
  buzzer.update();
  if (net.demoChanged) { st.demo = net.demoValue; net.demoChanged = false; if (!st.demo) { /* leave timers as-is */ } }

  uint32_t nowMs = millis();

  if (st.sensorError) {
    if (nowMs - lastRetryMs >= 2000) {
      lastRetryMs = nowMs; tryInit();
      disp.showError(faultName);
      net.update(st, false);
    }
    return;
  }

  // ---- 50 Hz motion sampling (fixed-rate with catch-up guard) -----------------
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - lastImuUs) >= IMU_PERIOD_US) {
    lastImuUs += IMU_PERIOD_US;
    if ((uint32_t)(nowUs - lastImuUs) > 100000UL) lastImuUs = nowUs;
    ImuSample s;
    if (sensors.readImu(s)) { imuFail = 0; if (sp.push(s.ax, s.ay, s.az)) onWindow(); }
    else if (++imuFail >= 10) enterError("MPU6050");
  }

  // ---- 1 Hz decision cycle -------------------------------------------------------
  if (nowMs - lastSecMs >= 1000) {
    lastSecMs += 1000;
    onSecond();
    if (!st.sensorError) {
      disp.showState(st, net.wifiUp(), net.mqttUp());
      net.update(st, riskEng.changed);
    }
  }
}
