#pragma once
// OLED output + non-blocking buzzer patterns.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "state.h"

class Display {
 public:
  bool ok = false;

  void begin() {
    ok = oled_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (!ok) return;
    oled_.clearDisplay(); oled_.setTextColor(SSD1306_WHITE);
    oled_.setTextSize(2); oled_.setCursor(4, 14); oled_.println("FieldGuard");
    oled_.setTextSize(1); oled_.setCursor(4, 40); oled_.println("MYOSA heat-risk monitor");
    oled_.display();
  }

  void showError(const char* sensor) {
    if (!ok) return;
    oled_.clearDisplay(); oled_.setTextColor(SSD1306_WHITE);
    oled_.setTextSize(2); oled_.setCursor(0, 8); oled_.println("Sensor Error");
    oled_.setTextSize(1); oled_.setCursor(0, 34); oled_.printf("Fault: %s", sensor);
    oled_.setCursor(0, 46); oled_.println("Retrying...");
    oled_.display();
  }

  void showState(const SystemState& s, bool wifi, bool mqtt) {
    if (!ok) return;
    oled_.clearDisplay(); oled_.setTextColor(SSD1306_WHITE); oled_.setTextSize(1);
    oled_.setCursor(0, 0);  oled_.printf("T:%.1fC P:%.0fhPa", s.tempEff, s.pressPa / 100.0f);
    oled_.setCursor(0, 10); oled_.printf("Act:%s", s.act == ACT_REST ? "Rest" : s.act == ACT_LIGHT ? "Light" : "Vigorous");
    oled_.setCursor(104, 10); oled_.printf("%c%c%c", wifi ? 'W' : '-', mqtt ? 'M' : '-', s.demo ? 'D' : ' ');
    oled_.setCursor(0, 20); oled_.printf("A:%um R:%um L:%u", (unsigned)(s.dact / 60), (unsigned)(s.drest / 60), s.light);
    oled_.setTextSize(2); oled_.setCursor(0, 31);
    oled_.print(s.risk == RISK_LOW ? "NORMAL" : s.risk == RISK_MEDIUM ? "CAUTION" : "WARNING");
    oled_.setTextSize(1); oled_.setCursor(0, 54);
    oled_.print(s.risk == RISK_LOW ? "Monitoring..." : s.risk == RISK_MEDIUM ? "Rest break advised" : "STOP-REST-SAFETY!");
    oled_.display();
  }

 private:
  Adafruit_SSD1306 oled_{128, 64, &Wire, -1};
};

class Buzzer {
 public:
  void begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(BUZZER_PIN, 2000, 8);
#else
    ledcSetup(0, 2000, 8); ledcAttachPin(BUZZER_PIN, 0);
#endif
    off();
  }
  // Non-blocking: `count` beeps of onMs with offMs gaps.
  void pattern(uint8_t count, uint16_t onMs, uint16_t offMs) {
    left_ = count; on_ = onMs; off_ = offMs; active_ = true; high_ = false; next_ = 0;
  }
  void update() {
    if (!active_) return;
    uint32_t now = millis();
    if ((int32_t)(now - next_) < 0) return;
    if (!high_) {
      if (left_ == 0) { active_ = false; return; }
      toneOn(); high_ = true; next_ = now + on_;
    } else {
      off(); high_ = false; left_--; next_ = now + off_;
    }
  }
 private:
  void toneOn() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(BUZZER_PIN, 2000);
#else
    ledcWriteTone(0, 2000);
#endif
  }
  void off() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(BUZZER_PIN, 0);
#else
    ledcWriteTone(0, 0);
#endif
  }
  uint8_t left_ = 0; uint16_t on_ = 0, off_ = 0; bool active_ = false, high_ = false; uint32_t next_ = 0;
};
