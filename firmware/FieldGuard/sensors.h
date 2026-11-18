#pragma once
// MYOSA sensor drivers: MPU6050 (raw I2C), BMP180, APDS9960 + validation.
#include <Wire.h>
#include <math.h>
#include <Adafruit_BMP085.h>      // BMP180 (Adafruit BMP085 Library)
#include <Adafruit_APDS9960.h>
#include "config.h"

struct ImuSample { float ax, ay, az, gx, gy, gz; };
struct EnvSample { float tempC; float pressPa; uint16_t clear; };

class Sensors {
 public:
  // Returns true only if ALL three sensors respond.
  bool begin() {
    mpuOk_ = initMpu();
    bmpOk_ = bmp_.begin();
    apdsOk_ = apds_.begin(10, APDS9960_AGAIN_1X);   // 10 ms integration, 1x gain (limits saturation)
    if (apdsOk_) apds_.enableColor(true);
    Wire.setClock(I2C_CLOCK);                       // libs may reset the clock
    return mpuOk_ && bmpOk_ && apdsOk_;
  }

  bool pingAll() {
    return ping(MPU_ADDR) && ping(0x77) && ping(0x39);
  }

  const char* faultName() const {
    if (!mpuOk_) return "MPU6050";
    if (!bmpOk_) return "BMP180";
    if (!apdsOk_) return "APDS9960";
    return "none";
  }

  // 50 Hz motion read. false => bus error or implausible value.
  bool readImu(ImuSample& s) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (uint8_t)true) != 14) return false;
    int16_t v[7];
    for (int i = 0; i < 7; i++) { int16_t hi = Wire.read(); v[i] = (hi << 8) | Wire.read(); }
    s.ax = v[0] / 8192.0f; s.ay = v[1] / 8192.0f; s.az = v[2] / 8192.0f;   // +-4 g
    s.gx = v[4] / 131.0f;  s.gy = v[5] / 131.0f;  s.gz = v[6] / 131.0f;    // +-250 dps
    if (fabsf(s.ax) > 4.5f || fabsf(s.ay) > 4.5f || fabsf(s.az) > 4.5f) return false;
    if (v[0] == 0 && v[1] == 0 && v[2] == 0) return false;                 // all-zero = dead bus
    return true;
  }

  // 1 Hz environment read with plausibility checks.
  bool readEnv(EnvSample& e) {
    float t = bmp_.readTemperature();
    float p = bmp_.readPressure();
    if (isnan(t) || isnan(p) || t < TEMP_MIN_C || t > TEMP_MAX_C ||
        p < PRESS_MIN_PA || p > PRESS_MAX_PA) return false;
    e.tempC = t; e.pressPa = p;
    uint16_t r, g, b, c;
    if (apds_.colorDataReady()) {
      apds_.getColorData(&r, &g, &b, &c);
      lastClear_ = c;
    }
    e.clear = lastClear_;
    return true;
  }

 private:
  bool ping(uint8_t addr) { Wire.beginTransmission(addr); return Wire.endTransmission() == 0; }
  bool wr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU_ADDR); Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
  }
  bool initMpu() {
    if (!ping(MPU_ADDR)) return false;
    bool ok = wr(0x6B, 0x01);        // wake, PLL clock
    delay(10);
    ok &= wr(0x1A, 0x04);            // DLPF ~21 Hz (below 25 Hz Nyquist @ 50 Hz)
    ok &= wr(0x1B, 0x00);            // gyro +-250 dps
    ok &= wr(0x1C, 0x08);            // accel +-4 g
    return ok;
  }

  Adafruit_BMP085 bmp_;
  Adafruit_APDS9960 apds_;
  bool mpuOk_ = false, bmpOk_ = false, apdsOk_ = false;
  uint16_t lastClear_ = 0;
};
