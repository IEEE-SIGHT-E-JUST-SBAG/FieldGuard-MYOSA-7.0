# Paper updates: IoT / MQTT + Flutter supervisor app

Drop-in replacements for the sections of the paper that change. Keep the local dashboard as an offline fallback.

## Abstract (replace the last two sentences before the disclaimer)
Results are shown on the OLED and signaled by the buzzer on the worker, and are streamed over the ESP32's Wi-Fi to an MQTT broker, from which a Flutter mobile application gives supervisors a live multi-worker view with alerts and history. When no internet is available, the device falls back to a local access-point dashboard served by the ESP32 itself. FieldGuard is an indicator-based awareness tool: it does not diagnose heat stress and does not measure WBGT.

## 1.2 (fix the sentence about the cloud)
Replace "cloud-based solutions assume internet access that rural fields often lack" with:
"…and purely cloud-based solutions stop working where connectivity is absent. FieldGuard therefore uses a hybrid design: all sensing, decision logic, and worker alerts run on the device, while connectivity, when available (for example a supervisor's phone hotspot), adds remote multi-worker monitoring."

## Table 1 (replace the Wi-Fi row)
| ESP32 Wi-Fi (on-board) | Comm. | MQTT telemetry to a broker (IoT link to the Flutter app) plus a local fallback dashboard. |

## Section 3 (replace item 8)
(8) publishes telemetry and alerts over MQTT and hosts a local fallback dashboard.
Add after the sentence "No electronic component from outside the MYOSA kit is used.":
"The broker and the supervisor's smartphone are software infrastructure, not hardware added to the wearable."

## Section 3.2 (replace the Wi-Fi sentence)
The kit's on-board Wi-Fi turns the wearable into an IoT node without extra communication hardware, and the OLED and buzzer cover local alerts.

## Software Development (replace the dashboard paragraph)
The firmware is split across the two ESP32 cores. Core 1 runs the 50 Hz sensing, feature extraction, activity classification, risk scoring, OLED, and buzzer. Core 0 runs a network task, so TLS handshakes and reconnections never disturb sampling. The device connects to Wi-Fi in station mode, publishes a JSON telemetry message about every 2 s and an immediate alert message when the risk level changes over MQTT (TLS), and reports presence with a retained status message and a Last-Will message. Topics follow `fieldguard/<deviceId>/{telemetry,alert,status,cmd}` (Table X). A Flutter application subscribes with a wildcard, so one app supervises several workers; it shows risk-sorted worker cards, live values, temperature and movement charts, and a visible and haptic alert when a worker enters Warning, and it can switch the device's Demo Mode through the `cmd` topic. Because fields often lack internet coverage, the access point and web server on the ESP32 remain available: a phone connected directly sees the same live values and a one-point-per-minute history stored in flash. Alerts at the worker (OLED and buzzer) never depend on connectivity.

## New subsection "IoT Communication" (after Software Development)
Table X: MQTT interface. `telemetry` (device→app, QoS 0, every 2 s): temperature, pressure, light category, activity class, RMS and variance, activity, rest and last-60-min rest durations, score terms, risk level, Demo and Sensor Error flags, Wi-Fi RSSI. `alert` (device→app): on a level change. `status` (retained, QoS 1, Last Will): online/offline. `cmd` (app→device, QoS 1): Demo Mode. A Sensor Error is published as an error state with no risk value, so supervisors are never shown a risk estimate based on missing data.
Limitations to state: the prototype does not buffer messages while offline (the on-device history is local only), and the prototype connects to the broker without server-certificate pinning.

## Figure 2 (update)
Add a block "MQTT broker" below "ESP32 Wi-Fi (on-board)", then "Flutter supervisor app (phone)". Keep "Local dashboard (fallback)" as a dashed branch.

## Section 6 demonstration
Stage 1: replace "a phone connected to the device's Wi-Fi shows the live dashboard" with "the Flutter app, connected through the broker, shows the live card; the local dashboard is shown as the offline fallback".
Stage 3: the Demo toggle can be switched from the app. Stage 4: also disconnect the broker/hotspot to show that the OLED and buzzer keep working and the app marks the device Offline (Last Will).

## Conclusion
Replace "offline Wi-Fi dashboard output" with "MQTT/IoT delivery to a mobile supervisor app with an offline local fallback", and in the future-work list replace "multi-worker supervision" with "store-and-forward buffering, authenticated per-device credentials, and larger multi-worker field deployments".

## New reference
[14] OASIS, "MQTT Version 3.1.1," OASIS Standard, 2014.
