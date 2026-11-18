#pragma once
// Pure C++: class confirmation + activity / rest timers (paper Sec. 5, fusion).
#include <stdint.h>
#include "config.h"
#include "types.h"

class ActivityTracker {
 public:
  ActivityClass confirmed = ACT_REST;
  uint32_t dact = 0;      // continuous activity duration (s), resets after 10 min rest
  uint32_t restRun = 0;   // current continuous rest (s)

  // A class change is accepted only after N consecutive windows.
  void onWindow(ActivityClass raw) {
    if (raw == cand_) cnt_++; else { cand_ = raw; cnt_ = 1; }
    if (cnt_ >= ACTIVITY_CONFIRM_WINDOWS) confirmed = cand_;
  }

  // Call once per (simulated) second.
  void tick1s() {
    simS_++;
    uint32_t idx = (simS_ / 60) % 60;
    if (idx != lastIdx_) { bins_[idx] = 0; lastIdx_ = idx; }
    if (confirmed == ACT_REST) {
      restRun++;
      bins_[idx]++;
      if (restRun >= REST_RESET_S) dact = 0;   // shorter pauses do not reset
    } else {
      restRun = 0;
      dact++;
    }
  }

  bool restRecognized() const { return restRun >= REST_RECOGNIZED_S; }
  uint32_t rest60() const { uint32_t s = 0; for (int i = 0; i < 60; i++) s += bins_[i]; return s; }

 private:
  ActivityClass cand_ = ACT_REST;
  int cnt_ = 0;
  uint32_t simS_ = 0, lastIdx_ = 0xFFFFFFFF;
  uint16_t bins_[60] = {0};
};
