# wiring/

รูปหรือแผนผังการต่อสาย หนึ่งไฟล์ต่อหนึ่งวงจร ตั้งชื่อตามโปรเจกต์ firmware เช่น `01-sensor-node.png`

## 01-sensor-node (ESP32 DevKit ↔ BME280)

| BME280 | ESP32 |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

ห้ามต่อ VIN เข้า 5V ถ้าโมดูลไม่มีวงจรลดแรงดันบนบอร์ด — ดูบนโมดูลก่อน
