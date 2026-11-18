#pragma once
#include <stdint.h>
#include "types.h"
#include "risk.h"

struct SystemState {
  float tempC = 0;          // calibrated (before demo offset)
  float tempEff = 0;        // value used by the risk algorithm
  float pressPa = 0;
  uint16_t clear = 0;
  uint8_t light = 0;
  ActivityClass act = ACT_REST;
  float rms = 0, var = 0;
  uint32_t dact = 0, drest = 0, rest60 = 0;
  RiskBreakdown rb{};
  RiskLevel risk = RISK_LOW;
  bool demo = false, sensorError = false, usedModel = false;
};
