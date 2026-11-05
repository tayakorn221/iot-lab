# platform/

ฝั่งรับข้อมูล: broker → ingest → DB → dashboard

| โฟลเดอร์ | ทำอะไร | สถานะ |
|---|---|---|
| `broker/` | config ของ Mosquitto | ✅ ทดสอบแล้ว |
| `ingest/` | Python subscribe `iotlab/v1/+/telemetry` และ `+/status` → ตรวจตาม contract → เขียน SQLite | ✅ ทดสอบแล้ว |
| `dashboard/` | FastAPI + หน้า HTML เดียว (ไม่พึ่ง CDN) แสดงสถานะโหนด + กราฟย้อนหลัง | ✅ ทดสอบแล้ว |

## รัน (4 หน้าต่าง)
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
4. dashboard (ครั้งแรก: `py -m pip install -r platform/dashboard/requirements.txt`) แล้วเปิด http://127.0.0.1:8080
```bash
py platform/dashboard/app.py
```

อยากดู dashboard ที่มีประวัติ 24 ชม. เลยโดยไม่ต้องรอ → สร้างฐานตัวอย่าง แล้วเปิด dashboard ชี้ไปที่ฐานนั้น:
```bash
py tools/seed-demo.py --db platform/data/demo.sqlite
```
```bash
py platform/dashboard/app.py --db platform/data/demo.sqlite --port 8081
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

## dashboard แสดงอะไร
| ส่วน | อ่านยังไง |
|---|---|
| การ์ดโหนด | ค่าล่าสุด · สถานะ ✓ ออนไลน์ / ▲ เงียบ (ออนไลน์แต่ไม่มีข้อมูลเข้าเกิน 60 วินาที) / ✕ ออฟไลน์ · % ข้อมูลครบใน 24 ชม. (เขียวเมื่อ ≥ 95% ตามเกณฑ์ด่าน 2) |
| แถบ "ข้อมูลเข้า" | หนึ่งแถบต่อโหนด ช่องว่าง = ช่วงที่ไม่มีข้อมูล — ใช้ตรวจเกณฑ์ด่าน 1 ข้อ "ช่องว่างมีแค่ช่วงที่ตั้งใจตัดเน็ต" |
| กราฟ 3 อัน | อุณหภูมิ · ความชื้น · ความกด แยกกราฟเพราะหน่วยต่างกัน · เส้นขาด = ไม่มีข้อมูล · ช่วงยาวเฉลี่ยเป็นช่อง (ไม่เกิน 720 จุดต่อเส้น) |
| ชี้เมาส์ | เส้นตั้งร่วมทุกกราฟ + ค่าของทุกโหนด ณ เวลานั้น |
| ตาราง | 40 ช่วงเวลาล่าสุด สำหรับอ่านตัวเลขตรง ๆ |

API: `GET /api/overview` · `GET /api/series?hours=1|6|24|168` · ตั้งช่วงส่งที่คาดหวังด้วย env `EXPECTED_INTERVAL_S` (ค่าเริ่ม 10)
dashboard เปิดฐานแบบอ่านอย่างเดียว ปิด/พังไปไม่กระทบ ingest

## ทดสอบ contract (ไม่ต้องมี broker)
```bash
py -m pytest platform/ingest -q
```

## ดูข้อความดิบใน broker
```bash
docker exec -it iotlab-broker mosquitto_sub -t "iotlab/#" -v
```

## กับดักที่เจอแล้ว
- **โหนดผีโผล่ใน dashboard** — สถานะ online/offline เป็นข้อความ retain broker จำไว้ตลอดและส่งให้ ingest ทุกครั้ง
  โหนดทดสอบที่เลิกใช้แล้วจึงโผล่ในฐานใหม่ · ล้างทีละ topic ด้วยการส่งข้อความว่างแบบ retain (ingest ข้ามข้อความว่างให้เอง):
  ```bash
  docker exec iotlab-broker mosquitto_pub -t iotlab/v1/fake-01/status -r -n
  ```
- **Docker Desktop ล้มตอนเปิด** ขึ้นข้อความ `sailor-ingest.sock … The file cannot be accessed by the system`
  สาเหตุคือไฟล์ socket ค้างจากรอบก่อน (ชื่อ "Ingest" เป็นบริการของ Docker เอง ไม่ใช่ ingest ของเรา)
  แก้: ปิด Docker → เปลี่ยนชื่อโฟลเดอร์ `%LOCALAPPDATA%\Docker\run` เป็น `run.stale-<วันที่>` → เปิดใหม่
  **อย่ากด Reset to factory defaults** — ล้าง image/container ทั้งเครื่อง
