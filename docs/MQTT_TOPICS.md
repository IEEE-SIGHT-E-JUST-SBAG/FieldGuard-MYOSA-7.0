# MQTT interface

All topics are under `fieldguard/<deviceId>/…` (`deviceId` = `fg-` + 3 MAC bytes, shown on the dashboard).
The Flutter app subscribes to `fieldguard/+/telemetry` and `fieldguard/+/status`, so **one app supervises many workers**.

| Topic | Dir | QoS / retain | Payload |
|---|---|---|---|
| `…/telemetry` | device → app | 0 / no, every 2 s | JSON below |
| `…/alert` | device → app | 0 / no, on level change | `{dev,name,risk,riskName,T,score,demo,ts}` |
| `…/status` | device → app | 1 / **retained**, LWT | `{"online":true}` / `{"online":false}` |
| `…/cmd` | app → device | 1 | `{"demo":true}` / `{"demo":false}` |

## Telemetry
```json
{"dev":"fg-A1B2C3","name":"Worker 1","fw":"1.0.0","ts":1763500000,"up":842,
 "err":false,"demo":false,"model":false,"rssi":-61,
 "T":31.4,"P":1007.9,"lux":1830,"light":1,"act":2,"rms":0.231,"var":0.0412,
 "dact":1820,"drest":0,"rest60":120,"st":1,"si":2,"sd":1,"sr":1,"sl":0,
 "score":5,"risk":2,"riskName":"Warning"}
```
`risk`: 0 Normal, 1 Caution, 2 Warning (`null` + `err:true` = Sensor Error, no estimate is published).
`act`: 0 Rest, 1 Light, 2 Moderate/Vigorous. `light`: 0 shade, 1 bright, 2 strong sun-like.
`ts` = 0 until NTP sync. Durations in seconds. `st/si/sd/sr/sl` = score terms from Table 3.
