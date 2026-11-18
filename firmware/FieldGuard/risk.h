#pragma once
// Pure C++ (no Arduino deps) so it can be unit-tested on a PC.
#include <stdint.h>
#include "config.h"
#include "types.h"

struct RiskInputs {
  float tempC;        // calibrated (+ demo offset)
  ActivityClass act;  // confirmed class
  uint32_t dactS;     // continuous activity duration
  uint32_t rest60S;   // rest seconds within last 60 min
  uint8_t light;      // 0 shade/indoor, 1 bright, 2 strong sun-like
};

struct RiskBreakdown { int st, si, sd, sr, sl, score; };

// Table 3 of the paper.
inline RiskBreakdown computeRisk(const RiskInputs& in) {
  RiskBreakdown b{};
  b.st = in.tempC >= TEMP_HIGH_C ? 2 : (in.tempC >= TEMP_CAUTION_C ? 1 : 0);
  b.si = (int)in.act;
  b.sd = in.dactS >= DACT_HIGH_S ? 2 : (in.dactS >= DACT_MED_S ? 1 : 0);
  b.sr = (in.dactS >= DACT_MED_S && in.rest60S < REST_MIN_S) ? 1 : 0;  // only after 30 min
  b.sl = in.light >= 2 ? 1 : 0;
  b.score = b.st + b.si + b.sd + b.sr + b.sl;
  return b;
}

inline RiskLevel levelFromScore(int s) {
  return s >= 5 ? RISK_HIGH : (s >= 3 ? RISK_MEDIUM : RISK_LOW);
}

// 3-cycle confirmation + 1-point hysteresis when stepping DOWN.
class RiskEngine {
 public:
  RiskLevel level = RISK_LOW, prev = RISK_LOW;
  bool changed = false;

  RiskLevel update(int score) {
    changed = false;
    RiskLevel target = levelFromScore(score);
    if (target < level) {                       // downgrade needs a margin of 1
      RiskLevel adj = levelFromScore(score + 1);
      target = adj < level ? adj : level;
    }
    if (target == level) { pending_ = level; count_ = 0; return level; }
    if (target == pending_) count_++; else { pending_ = target; count_ = 1; }
    if (count_ >= LEVEL_CONFIRM_CYCLES) {
      prev = level; level = target; count_ = 0; changed = true;
    }
    return level;
  }
  bool rose() const { return changed && level > prev; }

 private:
  RiskLevel pending_ = RISK_LOW;
  int count_ = 0;
};
