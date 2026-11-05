# iot-lab

ห้องทดลอง IoT สาย **device-to-cloud** — เซนเซอร์ → ESP32 → MQTT → ฐานข้อมูล → dashboard ครบในที่เดียว

## Why this exists
ฝึกฝั่งอุปกรณ์ให้ได้จริง ไม่ใช่ดูคลิป — ทุกด่านจบด้วยของที่ทำงานได้และมีเกณฑ์ผ่านที่ตอบได้แค่ "ผ่าน/ไม่ผ่าน" (ดู [ROADMAP.md](ROADMAP.md))

## โครง repo
| โฟลเดอร์ | ชั้น | มีอะไร |
|---|---|---|
| `firmware/` | อุปกรณ์ | โปรเจกต์ PlatformIO หนึ่งโฟลเดอร์ต่อหนึ่งงาน (`01-sensor-node/` …) |
| `platform/` | ฝั่งรับ | `docker-compose.yml` · `broker/` Mosquitto · `ingest/` MQTT→DB · `dashboard/` |
| `shared/` | สัญญากลาง | `mqtt-contract.md` — ชื่อ topic + หน้าตา payload ที่ firmware กับ platform ต้องตรงกัน |
| `hardware/` | ของจริง | `bom.md` รายการซื้อ · `wiring/` รูปต่อสาย |
| `tools/` | ตัวช่วย | สคริปต์ทดสอบ เช่น โหนดจำลอง (ทดสอบ platform ได้ก่อนบอร์ดมาถึง) |
| `docs/` | บันทึก | `decisions/` เหตุผลที่เลือก · `journal/` บันทึกรายด่าน (วัตถุดิบ handbook) |

**กฎข้อเดียวที่ห้ามแหก:** แก้ topic หรือ payload เมื่อไหร่ ให้แก้ `shared/mqtt-contract.md` ก่อน แล้วค่อยแก้โค้ดสองฝั่งตาม

## Quick Start (ฝั่ง platform)
```bash
cd platform
cp .env.example .env
docker compose up -d
```

## ด่าน
| ด่าน | ชื่อ | สถานะ |
|---|---|---|
| 1 | เส้นแรกครบเส้น | ⏳ รอบอร์ด |
| 2 | ใช้จริงในห้องทำงาน | — |
| 3 | เฟิร์มแวร์ระดับงาน | — |
| 4 | handbook ขึ้นเว็บ | — |
| 5 | (เลือก) สาย signal processing | — |

Last updated: 2026-11-05
