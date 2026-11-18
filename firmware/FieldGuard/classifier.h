#pragma once
// Activity classifier: Edge Impulse TinyML model with threshold baseline/fallback.
#include <string.h>
#include "config.h"
#include "types.h"
#ifdef USE_EDGE_IMPULSE
#include <FieldGuard_inferencing.h>   // exported Edge Impulse Arduino library
#endif

struct ClassResult { ActivityClass cls; float conf; bool usedModel; };

inline ActivityClass classifyThreshold(float rms) {
  if (rms < RMS_REST_G) return ACT_REST;
  if (rms < RMS_LIGHT_G) return ACT_LIGHT;
  return ACT_VIGOROUS;
}

#ifdef USE_EDGE_IMPULSE
static const float* g_eiInput = nullptr;
static int eiGetData(size_t offset, size_t length, float* out) {
  memcpy(out, g_eiInput + offset, length * sizeof(float));
  return 0;
}
static_assert(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE == WINDOW_N * 3,
              "Edge Impulse window must be 2 s @ 50 Hz, 3 axes (ax,ay,az in g)");
#endif

// interleaved: WINDOW_N*3 floats (ax,ay,az in g), rms: dynamic RMS of the window.
inline ClassResult classifyWindow(const float* interleaved, float rms) {
  ClassResult r{classifyThreshold(rms), 1.0f, false};
#ifdef USE_EDGE_IMPULSE
  g_eiInput = interleaved;
  signal_t sig; sig.total_length = WINDOW_N * 3; sig.get_data = &eiGetData;
  ei_impulse_result_t res;
  if (run_classifier(&sig, &res, false) == EI_IMPULSE_OK) {
    int best = 0;
    for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++)
      if (res.classification[i].value > res.classification[best].value) best = i;
    float conf = res.classification[best].value;
    const char* lb = res.classification[best].label;
    if (conf >= EI_MIN_CONFIDENCE) {                // else: threshold fallback
      if (!strcmp(lb, "rest")) r = {ACT_REST, conf, true};
      else if (!strcmp(lb, "light")) r = {ACT_LIGHT, conf, true};
      else if (!strcmp(lb, "vigorous")) r = {ACT_VIGOROUS, conf, true};
    }
  }
#endif
  return r;
}
