// Host-side unit tests for the pure-logic firmware modules.
// Build:  g++ -std=c++17 -I firmware/FieldGuard tests/test_logic.cpp -o test_logic && ./test_logic
#include <cassert>
#include <cmath>
#include <cstdio>
#include "types.h"
#include "risk.h"
#include "activity_tracker.h"
#include "signal_processing.h"

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static RiskLevel lvl(float T, ActivityClass a, uint32_t dact, uint32_t rest60, uint8_t light, int* score = nullptr) {
  RiskBreakdown b = computeRisk({T, a, dact, rest60, light});
  if (score) *score = b.score;
  return levelFromScore(b.score);
}

int main() {
  int s;
  // ---- scripted scenarios with expected levels (paper: verification table) ----
  CHECK(lvl(25, ACT_REST, 0, 0, 0, &s) == RISK_LOW && s == 0);
  CHECK(lvl(31, ACT_LIGHT, 0, 0, 0, &s) == RISK_LOW && s == 2);
  CHECK(lvl(31, ACT_VIGOROUS, 0, 0, 2, &s) == RISK_MEDIUM && s == 4);
  CHECK(lvl(36, ACT_VIGOROUS, 0, 0, 0, &s) == RISK_MEDIUM && s == 4);
  CHECK(lvl(36, ACT_VIGOROUS, 3700, 0, 2, &s) == RISK_HIGH && s == 8);
  CHECK(lvl(36, ACT_VIGOROUS, 2000, 900, 0, &s) == RISK_HIGH && s == 5);   // rest taken -> SR=0
  CHECK(lvl(31, ACT_VIGOROUS, 600, 0, 0, &s) == RISK_MEDIUM && s == 3);    // SR not before 30 min
  CHECK(lvl(29.9f, ACT_REST, 5000, 0, 0, &s) == RISK_MEDIUM && s == 3);    // SD2 + SR1

  // ---- hysteresis / confirmation ----------------------------------------------
  RiskEngine e;
  e.update(3); e.update(3); CHECK(e.level == RISK_LOW && !e.changed);
  e.update(3); CHECK(e.level == RISK_MEDIUM && e.changed && e.rose());
  for (int i = 0; i < 6; i++) e.update(2);                 // margin: 2 is not enough to drop
  CHECK(e.level == RISK_MEDIUM);
  for (int i = 0; i < 3; i++) e.update(1);
  CHECK(e.level == RISK_LOW);
  for (int i = 0; i < 3; i++) e.update(6);
  CHECK(e.level == RISK_HIGH);
  for (int i = 0; i < 5; i++) e.update(4);                 // 4 keeps High (needs <=3)
  CHECK(e.level == RISK_HIGH);
  for (int i = 0; i < 3; i++) e.update(3);
  CHECK(e.level == RISK_MEDIUM);

  // ---- activity confirmation + timers -----------------------------------------
  ActivityTracker t;
  t.onWindow(ACT_VIGOROUS); t.onWindow(ACT_VIGOROUS);
  CHECK(t.confirmed == ACT_REST);
  t.onWindow(ACT_VIGOROUS); CHECK(t.confirmed == ACT_VIGOROUS);
  for (int i = 0; i < 2400; i++) t.tick1s();
  CHECK(t.dact == 2400);
  for (int i = 0; i < 3; i++) t.onWindow(ACT_REST);
  for (int i = 0; i < 300; i++) t.tick1s();
  CHECK(t.dact == 2400 && t.restRecognized());             // short pause: no reset
  for (int i = 0; i < 300; i++) t.tick1s();
  CHECK(t.dact == 0 && t.rest60() == 600);                 // 10 min rest resets

  // ---- signal processing ---------------------------------------------------------
  SignalProc sp; int windows = 0;
  for (int i = 0; i < 400; i++) if (sp.push(0, 0, 1.0f)) windows++;
  CHECK(windows == 7 && sp.rms < 0.01f);                   // still: ~0 RMS
  sp.reset(); windows = 0;
  for (int i = 0; i < 400; i++) {
    float a = 1.0f + 0.3f * sinf(2 * M_PI * 2.0f * i / 50.0f);   // 2 Hz, 0.3 g amplitude
    if (sp.push(0, 0, a)) windows++;
  }
  printf("sine RMS = %.3f (expected ~0.212)\n", sp.rms);
  CHECK(sp.rms > 0.18f && sp.rms < 0.24f);

  printf(fails ? "\n%d FAILED\n" : "\nAll tests passed\n", fails);
  return fails;
}
