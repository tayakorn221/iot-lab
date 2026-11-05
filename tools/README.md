# tools/

| สคริปต์ | ทำอะไร | สถานะ |
|---|---|---|
| `fake-node.py` | โหนดจำลอง ส่ง payload ตาม contract — ทดสอบ ingest/dashboard ได้ก่อนบอร์ดมาถึง | ✅ |
| `seed-demo.py` | สร้างฐานตัวอย่างประวัติ 26 ชม. (2 โหนด มีช่วงหลุด) ผ่านโค้ด ingest — ดู dashboard ได้ทันที | ✅ |

## fake-node.py
```bash
py tools/fake-node.py
```
ตัวเลือกที่ใช้บ่อย:
- `--device-id fake-02` เปิดหลายหน้าต่างคนละ id = จำลองหลายโหนด
- `--interval 1 --count 30` ส่งเร็วและหยุดเอง
- `--no-time` ส่ง `ts=0` เหมือนบอร์ดที่ยังไม่ได้เวลาจาก NTP
- `--bad-every 5` ทุกข้อความที่ 5 ส่ง payload ผิด ไว้ดูว่า ingest ทิ้งถูก

ปิดด้วย Ctrl+C = ประกาศ `offline` เอง · ฆ่าโปรเซสทิ้ง = broker ส่ง Last Will `offline` แทน

⚠️ สถานะ `offline` ค้างอยู่ใน broker ถาวร (retain) — เลิกใช้ id ไหนแล้วล้างด้วย
`docker exec iotlab-broker mosquitto_pub -t iotlab/v1/<id>/status -r -n` ไม่งั้นจะโผล่เป็นโหนดผีใน dashboard

## seed-demo.py
```bash
py tools/seed-demo.py --db platform/data/demo.sqlite
```
ต้องระบุ `--db` เสมอ และสคริปต์ไม่ยอมเขียนลงฐานจริง (`platform/data/iotlab.sqlite`) · รันซ้ำ = ลบฐานตัวอย่างเดิมแล้วสร้างใหม่
