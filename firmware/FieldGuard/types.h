#pragma once
enum ActivityClass { ACT_REST = 0, ACT_LIGHT = 1, ACT_VIGOROUS = 2 };
enum RiskLevel { RISK_LOW = 0, RISK_MEDIUM = 1, RISK_HIGH = 2 };

inline const char* actName(ActivityClass a) {
  return a == ACT_REST ? "Rest" : a == ACT_LIGHT ? "Light" : "Moderate/Vigorous";
}
inline const char* riskName(RiskLevel r) {
  return r == RISK_LOW ? "Normal" : r == RISK_MEDIUM ? "Caution" : "Warning";
}
