# firmware/

หนึ่งโฟลเดอร์ = หนึ่งโปรเจกต์ PlatformIO ขึ้นต้นด้วยเลขด่าน

```
firmware/
  01-sensor-node/      ด่าน 1 — ESP32 + BME280 → MQTT
    platformio.ini
    src/main.cpp
    include/secrets.example.h   ← ก๊อปเป็น secrets.h แล้วใส่รหัสจริง (secrets.h ไม่ commit)
```

## ติดตั้ง toolchain (ครั้งเดียว)
```bash
py -m pip install --user platformio
```
หรือลงส่วนเสริม PlatformIO IDE ใน VS Code

## คำสั่งที่ใช้บ่อย (รันในโฟลเดอร์โปรเจกต์)
```bash
pio run -t upload
```
```bash
pio device monitor -b 115200
```

## กติกา
- ทุกโปรเจกต์ต้องส่ง topic/payload ตาม `../shared/mqtt-contract.md`
- log จาก serial monitor ที่อยากเก็บ → `../../_work/` ไม่ใส่ใน repo
