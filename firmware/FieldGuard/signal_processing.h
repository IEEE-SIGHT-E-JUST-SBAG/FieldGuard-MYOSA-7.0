#pragma once
// Pure C++: gravity removal (1 s moving average of |a|), window RMS / variance (Eq. 1).
#include <math.h>
#include <stdint.h>
#include "config.h"

class SignalProc {
 public:
  float rms = 0, var = 0;

  void reset() { head_ = 0; count_ = 0; since_ = 0; firstDone_ = false; maCount_ = 0; maHead_ = 0; maSum_ = 0; }

  // Push one sample (g). Returns true when a new 2 s window (50 % overlap) is ready.
  bool push(float ax, float ay, float az) {
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    if (maCount_ < MA_N) { ma_[maHead_] = mag; maSum_ += mag; maCount_++; }
    else { maSum_ -= ma_[maHead_]; ma_[maHead_] = mag; maSum_ += mag; }
    maHead_ = (maHead_ + 1) % MA_N;
    float dyn = mag - (float)(maSum_ / maCount_);

    raw_[head_][0] = ax; raw_[head_][1] = ay; raw_[head_][2] = az;
    dyn_[head_] = dyn;
    head_ = (head_ + 1) % WINDOW_N;
    if (count_ < WINDOW_N) count_++;
    if (count_ < WINDOW_N) return false;

    if (!firstDone_) { firstDone_ = true; since_ = 0; compute(); return true; }
    if (++since_ >= WINDOW_STRIDE) { since_ = 0; compute(); return true; }
    return false;
  }

  // Window in time order, interleaved ax,ay,az,... (for the Edge Impulse model).
  const float* interleaved() {
    for (int i = 0; i < WINDOW_N; i++) {
      int idx = (head_ + i) % WINDOW_N;
      out_[3 * i] = raw_[idx][0]; out_[3 * i + 1] = raw_[idx][1]; out_[3 * i + 2] = raw_[idx][2];
    }
    return out_;
  }

 private:
  void compute() {
    double mean = 0, sq = 0;
    for (int i = 0; i < WINDOW_N; i++) { mean += dyn_[i]; sq += (double)dyn_[i] * dyn_[i]; }
    mean /= WINDOW_N;
    double v = 0;
    for (int i = 0; i < WINDOW_N; i++) { double d = dyn_[i] - mean; v += d * d; }
    rms = (float)sqrt(sq / WINDOW_N);
    var = (float)(v / WINDOW_N);
  }

  float raw_[WINDOW_N][3];
  float dyn_[WINDOW_N];
  float out_[WINDOW_N * 3];
  float ma_[MA_N];
  double maSum_ = 0;
  int head_ = 0, count_ = 0, since_ = 0, maCount_ = 0, maHead_ = 0;
  bool firstDone_ = false;
};

template <int N>
class Smoother {
 public:
  float add(float v) {
    if (n_ < N) { b_[h_] = v; s_ += v; n_++; } else { s_ -= b_[h_]; b_[h_] = v; s_ += v; }
    h_ = (h_ + 1) % N;
    return s_ / n_;
  }
  void reset() { n_ = 0; h_ = 0; s_ = 0; }
 private:
  float b_[N];
  int n_ = 0, h_ = 0;
  float s_ = 0;
};
