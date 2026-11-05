# 0001 — เลือกเลนและ stack ของด่าน 1

สถานะ: ตกลงแล้ว · 2026-11-05

## เลน: device-to-cloud
กรแข็งฝั่ง data/web อยู่แล้ว (FastAPI, React, dashboard ใน CORE-MESH) จุดขาดคือฝั่งอุปกรณ์
คนที่ต่อได้ครบตั้งแต่เซนเซอร์ยันหน้าจอหายากกว่าคนเก่งชั้นเดียว

## stack ด่าน 1 — ทุกตัวเลือกให้ความยากไปกองอยู่ที่ firmware

| ชั้น | เลือก | ทำไม | เปลี่ยนเมื่อ |
|---|---|---|---|
| toolchain | **PlatformIO** (CLI ผ่าน pip หรือส่วนเสริม VS Code) | config เป็นไฟล์ `platformio.ini` อยู่ใน git ได้ · build ซ้ำได้ ไม่ขึ้นกับ IDE | — |
| framework | **Arduino core for ESP32** | ตัวอย่างเยอะสุด ด่านแรกต้องได้เส้นครบก่อน | ด่าน 3 ย้ายไป ESP-IDF |
| broker | **Mosquitto** ใน Docker | มาตรฐานวงการ เบา กรเคยเห็นใน CORE-MESH | — |
| ingest | **Python + paho-mqtt** | กรเขียน Python คล่อง ไม่ต้องเรียนของใหม่ฝั่งนี้ | — |
| DB | **SQLite** ไฟล์เดียว | ข้อมูลด่าน 1 วันละ ~8,640 แถว/โหนด เล็กมาก · ไม่ต้องดูแล server เพิ่ม | ด่าน 2 ถ้าข้อมูล/โหนดโต → พิจารณา TimescaleDB หรือ InfluxDB + Grafana |
| dashboard | **หน้า HTML เดียว + FastAPI ตัวเล็ก** | กรทำเป็นอยู่แล้ว เร็ว | พร้อมกับ DB ข้างบน |

## ที่ไม่เลือก
- **TIG stack (Telegraf + InfluxDB + Grafana) ตั้งแต่ด่าน 1** — ได้ 4 container กับ config เชื่อมกัน เวลาจะหมดกับการต่อ config แทนที่จะได้ฝึก firmware
- **Cloud IoT (AWS IoT Core ฯลฯ)** — ต้องจัดการ cert และบิล ยังไม่ใช่ปัญหาที่ด่านนี้ต้องแก้
