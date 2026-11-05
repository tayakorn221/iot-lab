# MQTT contract

**ไฟล์นี้คือความจริงหนึ่งเดียว** ระหว่าง `firmware/` กับ `platform/`
จะเปลี่ยนอะไร แก้ที่นี่ก่อน แล้วค่อยแก้โค้ดสองฝั่ง · เปลี่ยนแบบไม่เข้ากันของเดิม = ขึ้น `v` ใหม่

## Topic

```
iotlab/v1/<device_id>/telemetry   ค่าที่วัดได้        QoS 1   retain: ไม่
iotlab/v1/<device_id>/status      online / offline    QoS 1   retain: ใช่ (ตั้งเป็น Last Will)
```

- `device_id` = ตัวพิมพ์เล็ก a-z 0-9 และ `-` เท่านั้น เช่น `node-01`, `stg-room-a`
- ฝั่ง platform subscribe แบบ `iotlab/v1/+/telemetry` — เพิ่มโหนดใหม่ไม่ต้องแก้ platform

## Payload: telemetry

```json
{
  "ts": 1793865600,
  "seq": 42,
  "temp_c": 27.41,
  "rh_pct": 61.2,
  "pres_hpa": 1009.8
}
```

| field | ชนิด | หน่วย | บังคับ | หมายเหตุ |
|---|---|---|---|---|
| `ts` | int | Unix วินาที (UTC) | ใช่ | เวลาจากบอร์ด (ซิงก์ NTP) · ถ้าบอร์ดยังไม่ได้เวลา ส่ง `0` แล้ว ingest ใช้เวลาที่รับแทน |
| `seq` | int | ลำดับ | ใช่ | นับขึ้นทีละ 1 ตั้งแต่บูต — ไว้ดูว่าข้อความหายกี่อัน |
| `temp_c` | float | °C | ไม่ | |
| `rh_pct` | float | %RH | ไม่ | |
| `pres_hpa` | float | hPa | ไม่ | |

field ที่เซนเซอร์อ่านไม่ได้ **ไม่ต้องส่ง** ห้ามส่ง `0` แทน — `0` องศาเป็นค่าจริงได้

## Payload: status

ข้อความ plain text `online` หรือ `offline`
บอร์ดส่ง `online` แบบ retain ตอนต่อติด · ตั้ง Last Will เป็น `offline` แบบ retain
