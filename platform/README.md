# platform/

ฝั่งรับข้อมูล: broker → ingest → DB → dashboard

| โฟลเดอร์ | ทำอะไร | สถานะ |
|---|---|---|
| `broker/` | config ของ Mosquitto | ✅ ทดสอบแล้ว |
| `ingest/` | Python subscribe `iotlab/v1/+/telemetry` และ `+/status` → ตรวจตาม contract → เขียน SQLite | ✅ ทดสอบแล้ว |
| `dashboard/` | FastAPI + หน้า HTML เดียว แสดงกราฟย้อนหลัง | ⏳ |

## รัน (3 หน้าต่าง)
ต้องเปิด Docker Desktop ก่อน · รันจากโฟลเดอร์ `iot-lab/`

1. broker:
```bash
docker compose -f platform/docker-compose.yml up -d
```
2. ingest (ครั้งแรกลงไลบรารีก่อน: `py -m pip install -r platform/ingest/requirements.txt`):
```bash
py platform/ingest/ingest.py
```
3. โหนดจำลอง แทนบอร์ดที่ยังไม่มา:
```bash
py tools/fake-node.py
```

ข้อมูลไปอยู่ที่ `platform/data/iotlab.sqlite` (อยู่ใน `.gitignore`) · เปลี่ยนที่เก็บได้ด้วย env `DB_PATH`
เปิดดู log ละเอียดทุกข้อความด้วย env `LOG_LEVEL=DEBUG`

## ตาราง
| ตาราง | หนึ่งแถวคือ |
|---|---|
| `telemetry` | หนึ่งข้อความที่ผ่าน contract · `ts_source` บอกว่าเวลามาจากบอร์ด (`device`) หรือ ingest เติมให้ (`ingest`) · `raw` เก็บ payload ดิบ |
| `device_status` | หนึ่งโหนด · `updated_at` = เวลาที่สถานะเปลี่ยนครั้งล่าสุด |

## ความทนทานที่ทดสอบแล้ว (2026-11-05 ด้วย fake-node)
| สถานการณ์ | ผล |
|---|---|
| ดับ broker 16 วินาที | ingest ต่อกลับเองภายใน ~7 วินาทีหลัง broker ขึ้น · ข้อความช่วงหลุดตามมาครบ ไม่มี seq ขาด |
| ดับ ingest 10 วินาที | broker พักข้อความไว้ (persistent session) · เปิด ingest ใหม่ได้ครบ |
| payload ผิด contract | ทิ้งพร้อม log `WARNING ทิ้ง …` · ไม่ล้ม |
| โหนดหายแบบไม่ปิดดี ๆ | Last Will ทำงาน → `device_status` เป็น `offline` |

ยังไม่ได้ทดสอบกับบอร์ดจริง — เกณฑ์ด่าน 1 ใน `ROADMAP.md` นับเฉพาะบอร์ดจริง

## ทดสอบ contract (ไม่ต้องมี broker)
```bash
py -m pytest platform/ingest -q
```

## ดูข้อความดิบใน broker
```bash
docker exec -it iotlab-broker mosquitto_sub -t "iotlab/#" -v
```

## กับดักที่เจอแล้ว
- **Docker Desktop ล้มตอนเปิด** ขึ้นข้อความ `sailor-ingest.sock … The file cannot be accessed by the system`
  สาเหตุคือไฟล์ socket ค้างจากรอบก่อน (ชื่อ "Ingest" เป็นบริการของ Docker เอง ไม่ใช่ ingest ของเรา)
  แก้: ปิด Docker → เปลี่ยนชื่อโฟลเดอร์ `%LOCALAPPDATA%\Docker\run` เป็น `run.stale-<วันที่>` → เปิดใหม่
  **อย่ากด Reset to factory defaults** — ล้าง image/container ทั้งเครื่อง
