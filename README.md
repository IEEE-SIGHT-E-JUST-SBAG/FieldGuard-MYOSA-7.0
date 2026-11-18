# FieldGuard

MYOSA-based forearm wearable for **continuous environmental + physical-activity monitoring to support heat-stress risk awareness** in agricultural workers.
Indicator-based awareness tool - **not a diagnosis, not a WBGT measurement.**

```
MPU6050 + BMP180 + APDS9960 --I2C--> MYOSA ESP32 --> OLED + buzzer (on the worker)
                                        |  Wi-Fi
                                        +--> MQTT broker (cloud) --> Flutter supervisor app (multi-worker)
                                        +--> local AP dashboard (offline fallback, http://192.168.4.1)
```
The ESP32 is the only processor. Core 1 runs sensing/decision (50 Hz); core 0 runs a network task, so TLS/MQTT never disturbs sampling.

## Repo layout
| Path | What |
|---|---|
| `firmware/FieldGuard/` | Main Arduino sketch (sensing, features, TinyML hook, risk, OLED/buzzer, MQTT, local dashboard) |
| `firmware/DataCollection/` | Streams ax,ay,az (g) @ 50 Hz for Edge Impulse data forwarder |
| `flutter_app/` | Supervisor app (MQTT, live cards, charts, alerts, demo toggle) |
| `tests/test_logic.cpp` | Host unit tests for risk / timers / DSP (no hardware needed) |
| `tools/simulate_device.py` | Fake device for testing the app/broker without hardware |
| `docs/MQTT_TOPICS.md` | Topic + JSON contract |

## 1. Firmware
**Arduino IDE** (ESP32 core 2.x or 3.x), board "ESP32 Dev Module". Libraries: `Adafruit BMP085 Library`, `Adafruit APDS9960`, `Adafruit SSD1306`, `Adafruit GFX`, `PubSubClient`, `ArduinoJson` (v7).
1. `cp firmware/FieldGuard/secrets.example.h firmware/FieldGuard/secrets.h` and fill in Wi-Fi + broker.
2. **Verify `BUZZER_PIN` (and I2C pins) in `config.h` against the MYOSA motherboard documentation.**
3. Upload `firmware/FieldGuard`. Serial 115200.

**Connectivity in the field:** the ESP32 joins any Wi-Fi it can see (e.g. the supervisor's phone hotspot) and publishes over MQTT. If there is no internet the device keeps working: OLED + buzzer are local and a fallback access point `FieldGuard-XXXXXX` (password `AP_PASSWORD`) serves the offline dashboard at `http://192.168.4.1`, with history in flash.

## 2. Broker (free option: HiveMQ Cloud)
Create a Serverless cluster, add credentials, put host/user/password in `secrets.h` (port 8883, TLS). Any MQTT broker works (Mosquitto, EMQX). Prototype uses `setInsecure()`; set `MQTT_CA_CERT` for real deployments.

## 3. Flutter app
```bash
cd flutter_app
flutter create .          # generates android/ios folders (keeps lib/ and pubspec.yaml)
flutter pub get && flutter run
```
Enter host/port/user/password in Settings. Android release builds need `<uses-permission android:name="android.permission.INTERNET"/>` in `AndroidManifest.xml`. The app targets Android/iOS (raw TCP MQTT; web would need MQTT-over-WebSocket).
No hardware yet? `pip install paho-mqtt && python tools/simulate_device.py --host <broker> --user <u> --password <p>`.

## 4. TinyML (Edge Impulse)
1. Flash `firmware/DataCollection`, run `edge-impulse-data-forwarder --frequency 50`, name axes `ax,ay,az`, record labels `rest`, `light`, `vigorous` (plan in the paper: scripted tasks, split train/test **by session**).
2. Impulse: window 2000 ms / stride 1000 ms, flatten block (RMS, std), small NN, int8. Export as **Arduino library**; name it `FieldGuard_inferencing`.
3. Install the zip, uncomment `#define USE_EDGE_IMPULSE` in `config.h`. Low-confidence windows (<0.6) fall back to the threshold rule automatically.

## 5. Calibration (all numbers in `config.h` marked PROVISIONAL)
RMS thresholds (noise floor + 3σ; class-distribution crossing), BMP180 offset/slope vs reference thermometer (`TEMP_CAL_*`), APDS9960 shade/sun readings (`LIGHT_*`), battery runtime. Do the wrist-vs-forearm placement test **before** fixing RMS thresholds.

## 6. Tests
```bash
g++ -std=c++17 -I firmware/FieldGuard tests/test_logic.cpp -o test_logic && ./test_logic
```

## Status / honesty notes
* Pure-logic modules (risk, hysteresis, timers, DSP) are unit-tested on a PC. Firmware hardware paths and the Flutter app have **not** been run on a physical MYOSA board / device by the author of this scaffold - expect small fixes (pins, library versions).
* No Edge Impulse model is shipped (it must be trained on your own recordings).
* Temperature limits are dry-air bands, not validated heat-stress limits.
