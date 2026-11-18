#!/usr/bin/env python3
"""Fake FieldGuard device: lets you test the Flutter app / broker without hardware.
   pip install paho-mqtt
   python tools/simulate_device.py --host xxx.s1.eu.hivemq.cloud --user u --password p
"""
import argparse, json, random, time
import paho.mqtt.client as mqtt

ap = argparse.ArgumentParser()
ap.add_argument("--host", required=True); ap.add_argument("--port", type=int, default=8883)
ap.add_argument("--user", default=""); ap.add_argument("--password", default="")
ap.add_argument("--no-tls", action="store_true"); ap.add_argument("--dev", default="fg-SIM001")
ap.add_argument("--name", default="Sim Worker")
a = ap.parse_args()

c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, a.dev)
if a.user: c.username_pw_set(a.user, a.password)
if not a.no_tls: c.tls_set()
c.will_set(f"fieldguard/{a.dev}/status", json.dumps({"online": False}), 1, True)
demo = {"on": False}

def on_msg(_c, _u, m):
    try: demo["on"] = bool(json.loads(m.payload).get("demo", False))
    except Exception: pass

c.on_message = on_msg
c.connect(a.host, a.port, 30)
c.subscribe(f"fieldguard/{a.dev}/cmd")
c.publish(f"fieldguard/{a.dev}/status", json.dumps({"online": True}), 1, True)
c.loop_start()

temp, dact, drest, act, prev_risk = 27.0, 0, 0, 0, 0
t0 = time.time()
while True:
    step = 60 if demo["on"] else 1
    if random.random() < 0.05: act = random.choice([0, 1, 2, 2, 2])
    if act == 0: drest += step
    else: drest = 0; dact += step
    temp += random.uniform(-0.1, 0.2) + (0.05 if demo["on"] else 0)
    temp = max(24, min(40, temp))
    st = (2 if temp >= 35 else 1 if temp >= 30 else 0)
    sd = 2 if dact >= 3600 else 1 if dact >= 1800 else 0
    sr = 1 if dact >= 1800 else 0
    light = 2 if temp > 31 else 1
    score = st + act + sd + sr + (1 if light == 2 else 0)
    risk = 2 if score >= 5 else 1 if score >= 3 else 0
    msg = {"dev": a.dev, "name": a.name, "fw": "sim", "ts": int(time.time()), "up": int(time.time() - t0),
           "err": False, "demo": demo["on"], "model": False, "rssi": -60, "T": round(temp, 1),
           "P": 1008.0, "lux": 1500, "light": light, "act": act, "rms": round(0.05 + act * 0.12, 3),
           "var": 0.01, "dact": dact, "drest": drest, "rest60": 0, "st": st, "si": act, "sd": sd,
           "sr": sr, "sl": int(light == 2), "score": score, "risk": risk,
           "riskName": ["Normal", "Caution", "Warning"][risk]}
    c.publish(f"fieldguard/{a.dev}/telemetry", json.dumps(msg))
    if risk > prev_risk:
        c.publish(f"fieldguard/{a.dev}/alert", json.dumps({"dev": a.dev, "name": a.name, "risk": risk}))
    prev_risk = risk
    time.sleep(2)
