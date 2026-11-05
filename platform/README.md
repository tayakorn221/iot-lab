# platform/

ฝั่งรับข้อมูล: broker → ingest → DB → dashboard

| โฟลเดอร์ | ทำอะไร | สถานะ |
|---|---|---|
| `broker/` | config ของ Mosquitto | ⚠️ config พร้อม ยังไม่ได้ลองรันจริง |
| `ingest/` | Python subscribe `iotlab/v1/+/telemetry` → เขียน SQLite | ⏳ ทำระหว่างรอบอร์ด |
| `dashboard/` | FastAPI + หน้า HTML เดียว แสดงกราฟย้อนหลัง | ⏳ |

## รัน
```bash
cp .env.example .env
docker compose up -d
```

## ทดสอบ broker โดยไม่ต้องมีบอร์ด
หน้าต่างที่ 1 — ฟัง:
```bash
docker exec -it iotlab-broker mosquitto_sub -t "iotlab/#" -v
```
หน้าต่างที่ 2 — ส่ง:
```bash
docker exec iotlab-broker mosquitto_pub -t iotlab/v1/test-01/telemetry -m '{"ts":0,"seq":1,"temp_c":27.4}'
```

ไฟล์ SQLite ที่ ingest สร้างจะอยู่ที่ `platform/data/` (อยู่ใน `.gitignore`)
