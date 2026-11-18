#pragma once
// IoT layer: Wi-Fi (STA + fallback AP), MQTT publish/subscribe, local web dashboard,
// flash history. Runs in its own FreeRTOS task on core 0 so TLS handshakes and HTTP
// never disturb the 50 Hz sensing loop on core 1.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>
#include "config.h"
#include "secrets.h"
#include "state.h"

static const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>FieldGuard</title>
<style>body{font-family:system-ui;margin:0;background:#111;color:#eee;text-align:center}
#r{font-size:2.4rem;font-weight:700;padding:18px;margin:10px;border-radius:12px;background:#444}
.g{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:10px}.c{background:#222;padding:10px;border-radius:8px}
canvas{width:95%;height:120px;background:#1a1a1a;border-radius:8px}small{color:#999}</style></head><body>
<h3>FieldGuard <small id=dev></small></h3><div id=r>--</div><div class=g id=g></div>
<canvas id=cv width=600 height=240></canvas><br><small>Temperature history (1 point / min) &middot; offline local mode</small>
<script>
const col=['#2e7d32','#ef6c00','#c62828'],an=['Rest','Light','Moderate/Vigorous'];
async function tick(){try{const j=await (await fetch('/api')).json();
const r=document.getElementById('r');dev.textContent=j.dev;
r.textContent=j.err?'SENSOR ERROR':j.riskName+(j.demo?' (demo)':'');r.style.background=j.err?'#555':col[j.risk];
g.innerHTML=[['Temp',j.T+' C'],['Pressure',j.P+' hPa'],['Activity',an[j.act]],['Light',['Shade','Bright','Strong sun'][j.light]],
['Activity time',(j.dact/60|0)+' min'],['Rest time',(j.drest/60|0)+' min'],['RMS',j.rms+' g'],['Score',j.score]]
.map(x=>'<div class=c><small>'+x[0]+'</small><br><b>'+x[1]+'</b></div>').join('');}catch(e){}}
async function hist(){try{const t=(await (await fetch('/history.csv')).text()).trim().split('\n').slice(-120).map(l=>+l.split(',')[2]);
const c=cv.getContext('2d');c.clearRect(0,0,600,240);if(t.length<2)return;const mn=Math.min(...t)-1,mx=Math.max(...t)+1;
c.strokeStyle='#4fc3f7';c.beginPath();t.forEach((v,i)=>{const x=i*600/(t.length-1),y=240-(v-mn)/(mx-mn)*230-5;i?c.lineTo(x,y):c.moveTo(x,y)});c.stroke();}catch(e){}}
tick();hist();setInterval(tick,1000);setInterval(hist,30000);
</script></body></html>)HTML";

class Connectivity {
 public:
  char devId[16] = "";
  volatile bool demoChanged = false;
  volatile bool demoValue = false;

  void begin() {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(devId, sizeof devId, "fg-%06X", (unsigned)((mac >> 24) & 0xFFFFFF));
    snprintf(tTele_, sizeof tTele_, "fieldguard/%s/telemetry", devId);
    snprintf(tStat_, sizeof tStat_, "fieldguard/%s/status", devId);
    snprintf(tAlert_, sizeof tAlert_, "fieldguard/%s/alert", devId);
    snprintf(tCmd_, sizeof tCmd_, "fieldguard/%s/cmd", devId);

    mtx_ = xSemaphoreCreateMutex();
    LittleFS.begin(true);

    WiFi.mode(WIFI_AP_STA);                      // AP always on = offline fallback
    char ap[32]; snprintf(ap, sizeof ap, "FieldGuard-%s", devId + 3);
    WiFi.softAP(ap, AP_PASSWORD);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

#if MQTT_TLS
  #ifdef MQTT_CA_CERT
    net_.setCACert(MQTT_CA_CERT);
  #else
    net_.setInsecure();                          // prototype only: no server verification
  #endif
#endif
    mqtt_.setClient(net_);
    mqtt_.setServer(MQTT_HOST, MQTT_PORT);
    mqtt_.setBufferSize(768);
    mqtt_.setKeepAlive(30);
    mqtt_.setCallback([this](char*, uint8_t* p, unsigned int n) { onCmd(p, n); });

    server_.on("/", [this]() { server_.send_P(200, "text/html", DASH_HTML); });
    server_.on("/api", [this]() {
      char buf[640]; SystemState s; if (!snapshot(s)) { server_.send(503, "text/plain", "no data"); return; }
      buildJson(s, buf, sizeof buf); server_.send(200, "application/json", buf);
    });
    server_.on("/history.csv", [this]() {
      File f = LittleFS.open("/history.csv", "r");
      if (!f) { server_.send(404, "text/plain", "no history"); return; }
      server_.streamFile(f, "text/csv"); f.close();
    });
    server_.on("/demo", [this]() {
      demoValue = server_.arg("on") == "1"; demoChanged = true; server_.send(200, "text/plain", "ok");
    });
    server_.begin();
    xTaskCreatePinnedToCore(taskEntry, "net", 12288, this, 1, nullptr, 0);
  }

  // Called by the main loop every second. `changed` => publish an alert right away.
  void update(const SystemState& s, bool changed) {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    snap_ = s; have_ = true; if (changed) alertPending_ = true;
    xSemaphoreGive(mtx_);
  }

  bool wifiUp() const { return WiFi.status() == WL_CONNECTED; }
  bool mqttUp() const { return mqttUp_; }

 private:
  static void taskEntry(void* p) { ((Connectivity*)p)->run(); }
  void run() { for (;;) { service(); vTaskDelay(pdMS_TO_TICKS(5)); } }

  bool snapshot(SystemState& out, bool takeAlert = false, bool* alertOut = nullptr) {
    xSemaphoreTake(mtx_, portMAX_DELAY);
    bool h = have_; out = snap_;
    if (alertOut) { *alertOut = alertPending_; if (takeAlert) alertPending_ = false; }
    xSemaphoreGive(mtx_);
    return h;
  }

  void service() {
    server_.handleClient();
    uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
      if (!ntp_) { configTime(0, 0, "pool.ntp.org", "time.google.com"); ntp_ = true; }
      if (!mqtt_.connected()) {
        mqttUp_ = false;
        if (now - lastTry_ > 5000) { lastTry_ = now; connectMqtt(); }
      } else {
        mqttUp_ = true; mqtt_.loop();
      }
    } else mqttUp_ = false;

    SystemState s; bool alert = false;
    if (!snapshot(s, true, &alert)) return;

    if (mqttUp_) {
      if (alert) publishAlert(s);
      if (alert || now - lastPub_ >= PUBLISH_PERIOD_MS) { lastPub_ = now; publishTelemetry(s); }
    }
    if (now - lastHist_ >= HISTORY_PERIOD_MS && !s.sensorError) { lastHist_ = now; appendHistory(s); }
  }

  void connectMqtt() {
    char cid[24]; snprintf(cid, sizeof cid, "%s-%04X", devId, (unsigned)(esp_random() & 0xFFFF));
    bool ok = mqtt_.connect(cid, MQTT_USER, MQTT_PASS, tStat_, 1, true, "{\"online\":false}");
    if (ok) {
      mqtt_.publish(tStat_, "{\"online\":true}", true);   // retained
      mqtt_.subscribe(tCmd_, 1);
      mqttUp_ = true;
    }
  }

  void onCmd(uint8_t* p, unsigned int n) {
    JsonDocument d;
    if (deserializeJson(d, p, n)) return;
    if (!d["demo"].isNull()) { demoValue = d["demo"].as<bool>(); demoChanged = true; }
  }

  static float r1(float v) { return roundf(v * 10.0f) / 10.0f; }

  void buildJson(const SystemState& s, char* buf, size_t len) {
    JsonDocument d;
    time_t t = time(nullptr);
    d["dev"] = devId; d["name"] = WORKER_NAME; d["fw"] = FW_VERSION;
    d["ts"] = t > 1700000000 ? (uint32_t)t : 0;          // 0 = clock not synced yet
    d["up"] = (uint32_t)(millis() / 1000);
    d["err"] = s.sensorError; d["demo"] = s.demo; d["model"] = s.usedModel;
    d["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    if (s.sensorError) { d["risk"] = nullptr; d["riskName"] = "Sensor Error"; }
    else {
      d["T"] = r1(s.tempEff); d["P"] = r1(s.pressPa / 100.0f);
      d["lux"] = s.clear; d["light"] = s.light;
      d["act"] = (int)s.act; d["rms"] = roundf(s.rms * 1000) / 1000; d["var"] = roundf(s.var * 10000) / 10000;
      d["dact"] = s.dact; d["drest"] = s.drest; d["rest60"] = s.rest60;
      d["st"] = s.rb.st; d["si"] = s.rb.si; d["sd"] = s.rb.sd; d["sr"] = s.rb.sr; d["sl"] = s.rb.sl;
      d["score"] = s.rb.score; d["risk"] = (int)s.risk; d["riskName"] = riskName(s.risk);
    }
    serializeJson(d, buf, len);
  }

  void publishTelemetry(const SystemState& s) {
    char buf[640]; buildJson(s, buf, sizeof buf); mqtt_.publish(tTele_, buf);
  }

  void publishAlert(const SystemState& s) {
    JsonDocument d; char buf[256];
    d["dev"] = devId; d["name"] = WORKER_NAME; d["risk"] = (int)s.risk; d["riskName"] = riskName(s.risk);
    d["T"] = r1(s.tempEff); d["score"] = s.rb.score; d["demo"] = s.demo;
    time_t t = time(nullptr); d["ts"] = t > 1700000000 ? (uint32_t)t : 0;
    serializeJson(d, buf, sizeof buf); mqtt_.publish(tAlert_, buf);
  }

  void appendHistory(const SystemState& s) {
    File f = LittleFS.open("/history.csv", "r");
    if (f) { size_t sz = f.size(); f.close(); if (sz > HISTORY_MAX_BYTES) LittleFS.remove("/history.csv"); }
    f = LittleFS.open("/history.csv", "a");
    if (!f) return;
    time_t t = time(nullptr);
    f.printf("%lu,%lu,%.1f,%.1f,%u,%d,%.3f,%lu,%lu,%d\n",
             (unsigned long)(t > 1700000000 ? t : 0), (unsigned long)(millis() / 1000),
             s.tempEff, s.pressPa / 100.0f, s.light, (int)s.act, s.rms,
             (unsigned long)s.dact, (unsigned long)s.drest, (int)s.risk);
    f.close();
  }

#if MQTT_TLS
  WiFiClientSecure net_;
#else
  WiFiClient net_;
#endif
  PubSubClient mqtt_;
  WebServer server_{80};
  SemaphoreHandle_t mtx_ = nullptr;
  SystemState snap_;
  bool have_ = false, alertPending_ = false;
  volatile bool mqttUp_ = false;
  bool ntp_ = false;
  uint32_t lastTry_ = 0, lastPub_ = 0, lastHist_ = 0;
  char tTele_[48], tStat_[48], tAlert_[48], tCmd_[48];
};
