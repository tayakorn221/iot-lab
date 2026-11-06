# 01-sensor-node

ด่าน 1 — ESP32 อ่าน BME280 ทาง I2C แล้วส่งขึ้น MQTT ทุก 10 วินาที ตาม `../../shared/mqtt-contract.md` (v1)

**สถานะ:** `pio run` build ผ่าน (node-01 และ node-02) · **ยังไม่ได้ลองบนบอร์ดจริง** — ทดสอบตามเช็กลิสต์ท้ายไฟล์วันที่ของมาถึง

## ไฟล์

| ไฟล์ | ทำอะไร |
|---|---|
| `platformio.ini` | ล็อกเวอร์ชัน platform/library · หนึ่ง env ต่อหนึ่งบอร์ด (`node-01`, `node-02`) |
| `include/config.h` | ค่าตั้งที่ไม่ลับ: ขา, จังหวะส่ง, เวลา retry |
| `include/secrets.example.h` | แม่แบบ Wi-Fi/broker — ก๊อปเป็น `secrets.h` |
| `src/main.cpp` | วงหลัก: ดูแล Wi-Fi → ดูแล MQTT → ครบ 10 วิก็วัดแล้วส่ง → ไฟสถานะ |
| `src/net.cpp` | Wi-Fi หลายชุด (`WiFiMulti`), mDNS, NTP |
| `src/mqtt_link.cpp` | ต่อ broker, Last Will, `online`, ต่อใหม่เองแบบเว้นระยะ |
| `src/env_sensor.cpp` | BME280 — อ่าน chip ID, แยก BMP280 ปลอม, สายหลุดแล้วหาใหม่เอง |
| `src/telemetry.cpp` | struct หนึ่งรอบวัด → JSON (field ที่ไม่มีค่าไม่ใส่) |

## 1. ตั้ง secrets (ครั้งเดียวต่อเครื่องที่ build)

```bash
cp include/secrets.example.h include/secrets.h
```

แก้ `include/secrets.h`:
- `WIFI_NETWORKS` — ใส่ Wi-Fi บ้าน และ hotspot มือถือ (กับ Wi-Fi ที่ทำงานถ้าเป็นแบบรหัสเดียว) · บอร์ดสแกนแล้วเลือกตัวที่รู้จักและแรงสุดเอง
- ช่องที่สามของแต่ละ Wi-Fi = IP ของโน้ตบุ๊กที่รัน Mosquitto ในเครือข่ายนั้น (สำรองเผื่อ mDNS ไม่ทำงาน) · ไม่รู้ใส่ `""`
- `BROKER_MDNS_HOST` — ชื่อโน้ตบุ๊ก ดูด้วยคำสั่ง `hostname` (ไม่ต้องมี `.local`)

เช็กว่า git ไม่เห็นไฟล์นี้ — ต้องได้บรรทัดที่ขึ้นต้นด้วย `.gitignore`:

```bash
git check-ignore -v include/secrets.h
```

ไม่มี `secrets.h` ก็ยัง build ผ่าน (ใช้ค่าตัวอย่าง พร้อม warning) แต่บอร์ดจะต่อ Wi-Fi ไม่ติด

**Wi-Fi ที่ ESP32 ใช้ไม่ได้:** WPA2-Enterprise (ล็อกอินด้วย user+pass) · captive portal (หน้าเว็บให้กดยอมรับ) · 5 GHz อย่างเดียว
ที่ทำงานเป็นแบบนี้ → เปิด hotspot มือถือ แล้วให้ทั้งโน้ตบุ๊กและบอร์ดเกาะ hotspot เดียวกัน

**โน้ตบุ๊กต้องให้บอร์ดเข้าถึงได้:** Windows Firewall ต้องเปิดรับพอร์ต 1883 (TCP) และ 5353 (UDP, mDNS) ใน profile ของเครือข่ายนั้น
ถ้า Windows ตั้งเครือข่ายเป็น "Public" มักโดนบล็อกทั้งคู่ — เปลี่ยนเป็น "Private" สำหรับ Wi-Fi บ้าน/hotspot ตัวเอง

## 2. ต่อสาย 4 เส้น

โมดูล Waveshare BME280 มี 6 ขา ใช้แค่ 4 ขา (สายที่แถมมาเป็นหัวเมีย เสียบขาบอร์ดตรงได้):

| BME280 (Waveshare) | ESP32 | หมายเหตุ |
|---|---|---|
| VCC | `3V3` | โมดูลนี้รับ 5V ได้ แต่ใช้ 3V3 ไว้ก่อน — ระดับสัญญาณตรงกับ ESP32 |
| GND | `GND` | |
| SDA | `GPIO 21` (`D21`) | |
| SCL | `GPIO 22` (`D22`) | |
| ADDR, CS | ไม่ต่อ | ปล่อย ADDR ว่าง = address `0x77` (ค่าตั้งต้นของ Waveshare) · ต่อลง GND = `0x76` · เฟิร์มแวร์หาเองทั้งสองค่า |

ถอด USB ทุกครั้งก่อนย้ายสาย · บอร์ด 38 ขากว้างจนเต็ม breadboard → เสียบสายตรงจากขาบอร์ด ไม่ต้องผ่าน breadboard

## 3. แฟลช

ครั้งแรกบน Windows: ลงไดรเวอร์ **CP210x** (Silicon Labs) → เสียบบอร์ดแล้วต้องเห็น `Silicon Labs CP210x ... (COMx)` ใน Device Manager
ไม่เห็นเลย = สาย USB ชาร์จอย่างเดียว หรือไดรเวอร์ยังไม่ลง

บอร์ดแรก (`node-01`):

```bash
pio run -t upload
```

```bash
pio device monitor
```

บอร์ดที่สอง:

```bash
pio run -e node-02 -t upload
```

upload ค้างที่ `Connecting....___` → กดปุ่ม `BOOT` ค้างไว้จนเห็นเปอร์เซ็นต์ขึ้น แล้วปล่อย
log ที่อยากเก็บไว้ดู → บันทึกลง `../../../_work/` ไม่ใส่ใน repo

## 4. อ่านสถานะ

ไฟสีฟ้าบนบอร์ด (GPIO 2) — ไม่ต้องต่อ serial ก็รู้ว่าค้างตรงไหน:

| ไฟ | ความหมาย |
|---|---|
| กะพริบเร็ว 5 ครั้ง/วิ | กำลังหา Wi-Fi |
| กะพริบช้า 1 ครั้ง/วิ | ได้ Wi-Fi แล้ว ต่อ broker ไม่ติด |
| ติดค้าง ดับวูบทุก 10 วิ | ส่งข้อมูลปกติ (ดับวูบ = ส่งสำเร็จหนึ่งข้อความ) |

serial monitor ขึ้นต้นด้วยแท็ก `[sensor]` `[wifi]` `[mdns]` `[ntp]` `[mqtt]` `[pub]` · บรรทัดแรกบอก `reset reason`
— ถ้าเห็น `BROWNOUT` แปลว่าไฟเลี้ยงไม่พอ (สาย USB ยาว/บาง หรือที่ชาร์จในรถ)

## 5. พฤติกรรมตอนเน็ตหลุด

- **Wi-Fi หาย:** ตัด MQTT ทิ้งทันที แล้วสแกนหา Wi-Fi ที่รู้จักทุก 5 วิ · ได้ Wi-Fi คืนแล้วต่อ broker ทันทีไม่รอ
- **broker ดับ:** ต่อใหม่เว้นระยะ 2 → 4 → 8 → 15 วิ (เพดาน 15 วิ) · สลับระหว่าง mDNS กับ IP สำรอง
- **ระหว่างหลุด** ยังวัดและนับ `seq` ต่อ แต่ทิ้งข้อความ — `seq` ที่ขาดใน DB คือช่วงที่หลุด (ด่าน 3 ค่อยเก็บค้างไว้ส่งทีหลัง)
- **broker ประกาศ `offline` แทนบอร์ด** ผ่าน Last Will หลังบอร์ดเงียบ ~45 วิ (keepalive 30 วิ × 1.5)
- **ทางหนีสุดท้าย:** ไม่ได้ต่อ broker นาน 30 นาที → รีบูตตัวเอง (`seq` เริ่มที่ 1 ใหม่) · ตั้งยาวกว่าการทดสอบปิดเราเตอร์ 5 นาทีมาก จะได้ไม่บังบั๊ก

**ข้อจำกัดที่รู้อยู่:** ช่วงไม่มี Wi-Fi `loop()` ติดอยู่ใน `WiFiMulti.run()` ได้ถึง ~8 วิต่อรอบ (สแกน ~2 วิ + รอต่อ 6 วิ)
และต่อ broker ไม่ติดค้างได้ ~3–5 วิต่อครั้ง — ไม่กระทบการส่ง เพราะช่วงนั้นไม่มีทางส่งอยู่แล้ว · ด่าน 3 แยกเป็น FreeRTOS task

## 6. เช็กลิสต์วันของถึง

ทำตามลำดับ ข้อก่อนหน้ายังไม่ผ่านอย่าข้าม — พังตรงไหนจะได้รู้ว่าพังที่ชั้นไหน

**ฮาร์ดแวร์ (ยังไม่ต่อ BME280)**
- [ ] เสียบ USB แล้ว Device Manager เห็น `CP210x (COMx)`
- [ ] `pio run -t upload` ผ่าน · serial ขึ้นแบนเนอร์ `device_id=node-01` และ `reset reason: power on`
- [ ] ไฟสีฟ้ากะพริบ (เร็ว = ยังหา Wi-Fi) · serial ขึ้น `[sensor] no BME280` — ถูกแล้ว ยังไม่ได้ต่อ

**เซนเซอร์** (ถอด USB → ต่อ 4 สาย → เสียบใหม่)
- [ ] serial ขึ้น `chip ID = 0x60` และ `OK: genuine BME280` · ถ้าขึ้นกรอบ `!!! chip ID 0x58` = ได้ BMP280 ปลอม เคลมร้าน
- [ ] บรรทัด `[pub]` มี `temp_c` ใกล้อุณหภูมิห้อง และ `rh_pct` ไม่ใช่ 0 หรือ 100

**เส้นข้อมูล** (รัน broker + ingest + dashboard ตาม `../../platform/README.md`)
- [ ] serial ขึ้น `[wifi] up` แล้ว `[mqtt] connecting ... (mDNS)` แล้ว `connected` · ไฟติดค้างดับวูบทุก 10 วิ
- [ ] `docker exec -it iotlab-broker mosquitto_sub -t "iotlab/#" -v` เห็น `status online` และ telemetry ทุก 10 วิ ทุก field ตรง contract
- [ ] ภายในนาทีแรก serial ขึ้น `[ntp] time synced` และ `ts` ในข้อความไม่เป็น `0` อีก
- [ ] dashboard เห็นเส้นของ `node-01`

**ทนเน็ตหลุด** (เกณฑ์ผ่านด่าน 1)
- [ ] ปิดเราเตอร์ 5 นาที → `mosquitto_sub` เห็น `offline` ภายใน ~45 วิ → เปิดเราเตอร์ → จับเวลาจากไฟ Wi-Fi เราเตอร์ติด ถึง telemetry ข้อความแรก ต้อง **≤ 60 วิ** โดยไม่แตะบอร์ด
- [ ] `docker stop iotlab-broker` → ไฟบอร์ดกะพริบช้า → `docker start iotlab-broker` → บอร์ดส่งต่อเองภายใน ~15 วิ
- [ ] ดึงสาย SDA ออกระหว่างรัน → ข้อความยังส่งแต่ไม่มี `temp_c`/`rh_pct`/`pres_hpa` (ไม่ใช่ 0) → เสียบกลับ → ค่ากลับมาเอง

**ย้ายที่** (บ้าน ↔ ที่ทำงาน)
- [ ] ปิด Wi-Fi บ้าน เปิด hotspot มือถือ (โน้ตบุ๊กเกาะ hotspot ด้วย) → บอร์ดย้ายไปเองโดยไม่แฟลชใหม่ · ถ้า mDNS ไม่เจอ ใส่ IP สำรองของ hotspot ใน `secrets.h`
- [ ] `git log -p | grep -i <ssid บ้าน>` ว่างเปล่า

ผลทดสอบจริง → บันทึกลง `../../docs/journal/01-first-line.md`

## 7. ต่อ PMS5003 / จอ OLED ทีหลัง (หลังเกณฑ์ด่าน 1 ผ่าน)

โครงเตรียมไว้แล้ว — `Telemetry` มี `pm1_ugm3` `pm25_ugm3` `pm10_ugm3` อยู่ และ `telemetryToJson()` ส่งให้เองเมื่อค่า ≥ 0

1. เพิ่ม `src/dust_sensor.{h,cpp}` แบบเดียวกับ `env_sensor` — `begin()` เปิด `Serial2` 9600 bps, `read(Telemetry&)` เติม field `pm*`
2. อ่าน frame แบบไม่ block (เก็บ byte ที่เข้ามาทุกรอบ `loop()`) · ใช้ชุด atmospheric (byte 10–15) · checksum ไม่ผ่านหรือ 30 วิแรกหลังเปิดพัดลม → ไม่เติม field
3. ใน `main.cpp` ตรงคอมเมนต์ `ด่าน 1+` — สร้าง object แล้วเรียก `read(t)` ต่อจาก `envSensor.read(t)`
4. จอ OLED (SSD1306 `0x3C`) อยู่บน `Wire` ตัวเดียวกับ BME280 ได้เลย ไม่ชน address
