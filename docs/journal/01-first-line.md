# ด่าน 1 — เส้นแรกครบเส้น

สถานะ (18 พ.ย. 69): **ส่วนฮาร์ดแวร์ผ่าน · เส้นข้อมูลครบเส้นแล้ว** (บอร์ด → KMUTT-Secure → Mosquitto → ingest → SQLite → dashboard)

| เกณฑ์ด่าน 1 (`ROADMAP.md`) | สถานะ |
|---|---|
| payload ตรง `shared/mqtt-contract.md` ทุก field | ✅ 18 พ.ย. 69 |
| ปิดเราเตอร์ 5 นาที → กลับมาส่งเองใน 60 วิ | ⏳ ปิด AP มหาลัยไม่ได้ — ต้องทดสอบด้วย hotspot มือถือ (ยังไม่ได้ใส่ใน `secrets.h`) |
| ถอดปลั๊ก broker → ingest ต่อกลับเอง | ✅ 18 พ.ย. 69 |
| dashboard ย้อนหลัง 24 ชม. ช่องว่างมีแค่ช่วงที่ตั้งใจตัด | ⏳ ต้องรันต่อเนื่อง 24 ชม. |
| รหัส Wi-Fi ไม่อยู่ในประวัติ git | ⚠️ เกณฑ์เขียนให้ grep SSID แต่ `KMUTT-Secure` (ชื่อสาธารณะ) อยู่ใน commit แล้ว · ที่เป็นความลับคือ username/password — ตรวจทั้งประวัติทุก branch ไม่เจอ · ควรแก้เกณฑ์ใน ROADMAP |

---

## 18 พ.ย. 69 — บอร์ดแรกขึ้นจริง

### ทำอะไร

ของ: NodeMCU ESP32 30 ขา (Cytron · ชิป USB = CP2102) + BME280 ของ Waveshare พร้อมสาย PH2.0 6 เส้น
เฟิร์มแวร์: `firmware/01-sensor-node` env `node-01` v0.1.0 · ใช้ `secrets.h` ที่ใส่ Wi-Fi ไว้แล้ว

1. เสียบบอร์ดเปล่า (ยังไม่ต่อเซนเซอร์) → Device Manager → `Silicon Labs CP210x USB to UART Bridge (COM5)`
2. แฟลช — จบ `[SUCCESS]` ใน 21 วิ (842,000 ไบต์ · hash ตรง)
   ```bash
   pio run -e node-01 -t upload --upload-port COM5
   ```
3. อ่าน serial: แบนเนอร์ `device_id=node-01` · `reset reason: power on` · `[sensor] no BME280` (ถูก — ยังไม่ต่อ) · ไฟสีฟ้ากะพริบเร็ว (หา Wi-Fi)
   ```bash
   pio device monitor -p COM5 -b 115200
   ```
4. ถอด USB → ต่อ 4 สาย แดง→`3V3` · ดำ→`GND` · น้ำเงิน→`D21` (SDA) · เหลือง→`D22` (SCL) · ส้ม (ADDR) กับเขียว (CS) ปล่อยลอย → เสียบ USB
5. serial: `[sensor] I2C 0x77 chip ID = 0x60` → `OK: genuine BME280 at 0x77` · บรรทัด `[pub]` มี `temp_c` `rh_pct` `pres_hpa`
6. ทดสอบว่าเซนเซอร์ตอบจริง: เอามือจับเซนเซอร์ → ค่าพุ่งทันทีรอบถัดไป (ตารางใน "ผล")

### พังตรงไหน

| อาการ | สาเหตุจริง | แก้ |
|---|---|---|
| Device Manager มี `CP2102 USB to UART Bridge Controller` แต่สถานะ Error · ไม่มี COM | Code 28 = เครื่องไม่มีไดรเวอร์ CP210x เลย (สาย USB ไม่ผิด — ถ้าสายชาร์จอย่างเดียว จะไม่เห็นชิปเลย) | ลง **CP210x Universal Windows Driver** จาก silabs.com: แตก zip → คลิกขวา `silabser.inf` → Install → ถอดเสียบใหม่ → ขึ้น COM5 |
| upload ขึ้น `could not open port 'COM5': PermissionError(13, 'Access is denied.')` | เปิด `pio device monitor` ค้างไว้ในหน้าต่างอื่น — พอร์ต COM เปิดได้ทีละโปรแกรม | ปิด monitor (Ctrl+C) ก่อน upload ทุกครั้ง |
| monitor ขึ้นตัวอักษรขยะอ่านไม่ออก | รัน `pio device monitor` นอกโฟลเดอร์โปรเจกต์ → ไม่อ่าน `monitor_speed` จาก `platformio.ini` → ใช้ 9600 แต่บอร์ดพูด 115200 | ใส่ `-b 115200` เสมอ |
| ต่อเซนเซอร์แล้วยังขึ้น `no BME280 at 0x76/0x77` | ปลายสายหัวเมียฝั่ง ESP32 ยังไม่ได้เสียบเข้าขาบอร์ด (เห็นจากรูปที่ถ่ายมา) · ระหว่างนั้นเข้าใจผิดว่าปลั๊กฝั่งเซนเซอร์เสียบไม่เข้า — ที่จริงปลั๊ก PH2.0 เสียบอยู่แล้ว ปลั๊กแบบนี้แน่นมาก และเสียบสุดแล้วตัวปลั๊กยังโผล่พ้นช่องบางส่วนเป็นปกติ | เสียบ 4 เส้นเข้า 3V3/GND/D21/D22 → เจอทันทีที่ `0x77` |

ของที่ดูเหมือนพังแต่ไม่ได้พัง:
- `[E][Wire.cpp:499] requestFrom(): i2cWriteReadNonStop returned Error -1` — log ของไลบรารีตอนเรียกหาเซนเซอร์แล้วไม่มีใครตอบ · ตอนต่อเซนเซอร์แล้วยังเหลือ 1 บรรทัดตอนบูต เพราะเฟิร์มแวร์ลอง `0x76` ก่อนแล้วค่อยเจอที่ `0x77` (โมดูล Waveshare ค่าตั้งต้นเป็น `0x77`)
- `[E][WiFiMulti.cpp:191] run(): [WIFI] no matching wifi found!` — Wi-Fi ที่ตั้งไว้ไม่มีตัวไหนอยู่ในระยะ (วันนี้ทำที่ที่ไม่มี Wi-Fi ของเรา)
- บรรทัด `[E]` ทั้งสองโผล่เพราะ `CORE_DEBUG_LEVEL=1` ใน `platformio.ini` — ตั้งใจเปิดไว้ให้เห็น error ของ core
- monitor ขึ้น `#0 0x40080400 in _init at ??:?` ใต้บรรทัด `load:0x40080400` ตอนบูต — filter `esp32_exception_decoder` อ่านเลขในข้อความบูตของชิปผิดว่าเป็น crash · ไม่ใช่ crash
- ระยะห่างระหว่าง `[pub]` ไม่เท่ากันช่วงหา Wi-Fi ไม่เจอ (เช่น seq 1→2 ห่าง 11.4 วิ แล้ว 2→3 ห่าง 4.1 วิ) — `WiFiMulti.run()` บล็อก `loop()` หลายวินาที รอบถัดไปจึงชดเชยกลับเข้ากำหนดเดิม · ข้อจำกัดที่รู้อยู่ (README ข้อ 5) แก้ถาวรในด่าน 3

### ผล

ค่าในห้อง (บอร์ดไม่มีเน็ต `ts` จึงเป็น `0` ทุกข้อความ — ถูกแล้ว):

| ช่วง | `temp_c` | `rh_pct` | `pres_hpa` |
|---|---|---|---|
| ปล่อยนิ่ง | 27.0–29.1 | 44.6–52.6 | 1010.6–1010.7 |

ทดสอบเอามือจับเซนเซอร์ (เวลาจาก monitor):

| seq | เวลา | `temp_c` | `rh_pct` | |
|---|---|---|---|---|
| 15 | 17:31:59 | 27.01 | 52.6 | ก่อนจับ |
| 16 | 17:32:11 | 29.36 | 68.8 | มือแตะ |
| 17 | 17:32:22 | 31.47 | 54.2 | |
| 18 | 17:32:34 | 30.43 | 45.6 | ปล่อยมือ — อุ่นค้าง ความชื้นสัมพัทธ์จึงต่ำกว่าเดิม |
| 19 | 17:32:45 | 32.02 | 70.2 | จับอีกรอบ |
| 21 | 17:32:59 | 31.96 | 44.1 | |

ข้อสังเกต: อากาศอุ่นขึ้นรับไอน้ำได้มากขึ้น ไอน้ำเท่าเดิม `rh_pct` จึงลด — เรื่องนี้กลับมาตอนด่าน 6 (PM2.5 อ่านเกินจริงตอนความชื้นสูง)

ไฟล์ประกอบ (ไม่อยู่ใน repo):
- log serial: `../../../_work/serial/2026-11-18-node01-bare.log` (ยังไม่ต่อเซนเซอร์) · `../../../_work/serial/2026-11-18-node01-bme280.log` (ต่อแล้ว)
- คู่มือประกอบพร้อมรูป: `../../../_build/guide-01/stage1-assembly.html`

### ถัดไป (เส้นข้อมูล)

ต้องเคลียร์ฝั่งโน้ตบุ๊กก่อน:
- Wi-Fi ที่ทำงาน `KMUTT-Secure` เป็น WPA2-Enterprise (PEAP) — ชิป ESP32 เข้าได้ แต่เฟิร์มแวร์ v0.1.0 ยังไม่รองรับ → เพิ่มใน v0.2.0 (branch `firmware/wifi-enterprise`) · ทางสำรองคือ hotspot มือถือ
- Docker Desktop เปิดไม่ขึ้น (socket `sailor-ingest.sock` ค้าง — วิธีแก้ใน `platform/README.md`)
- Windows Firewall ยังไม่เปิดรับพอร์ต 1883

---

## 18 พ.ย. 69 (ต่อ) — เฟิร์มแวร์ v0.2.0 เข้า KMUTT-Secure

### ทำอะไร

- เพิ่ม WPA2-Enterprise (PEAP) ใน `net.cpp` แทน `WiFiMulti` · ใส่บัญชีใน `secrets.h` (`HAS_WIFI_EAP 1`) · แฟลช node-01
- ยังไม่ได้ใส่ใบรับรอง CA — serial เตือน `Enterprise server certificate NOT verified` ทุกครั้งที่บูต

### ผล

| ข้อ | ผล |
|---|---|
| เข้า Wi-Fi | ✅ `KMUTT-Secure` · RSSI −44 ถึง −47 dBm · ได้ IP ในวงเดียวกับโน้ตบุ๊ก (subnet /22) |
| ออกเน็ตได้ | ✅ `[ntp] time synced` ภายใน ~10 วิ · `ts` ไม่เป็น 0 แล้ว |
| mDNS หาโน้ตบุ๊ก | ✅ ชื่อโน้ตบุ๊ก `.local` ได้ IP ตรงกับ IP จริงของโน้ตบุ๊ก |
| client isolation | ✅ ไม่มี — `ping` จากโน้ตบุ๊กไปบอร์ดได้ 4/4 (107–169 ms) |
| ต่อ broker | ❌ `[mqtt] connect failed: err -3` — บนโน้ตบุ๊กยังไม่มีอะไรฟังพอร์ต 1883 (Docker ยังเปิดไม่ขึ้น) |

ของที่ดูเหมือนพังแต่ไม่ได้พัง: บรรทัดแรกหลัง `joining "KMUTT-Secure"` ขึ้น `disconnected, reason 202 wrong password?` แล้วบรรทัดถัดมาได้ IP เลย — รอบ EAP แรกโดนปัดแล้ว driver ลองซ้ำเองสำเร็จ · ถ้ารหัสผิดจริงจะไม่มี `got IP` ตามมา

log: `../../../_work/serial/2026-11-18-node01-v020-kmutt.log`

---

## 18 พ.ย. 69 (ต่อ) — เส้นข้อมูลครบเส้น + ทดสอบดับ broker

### ทำอะไร

1. Docker Desktop ล้มด้วย socket `sailor-ingest.sock` ค้าง (รอบที่ 3) → ปิด Docker → เปลี่ยนชื่อ `%LOCALAPPDATA%\Dockerun` เป็น `run.stale-20261118` **จาก PowerShell ที่เปิดจาก Start menu** → เปิด Docker ใหม่ ขึ้นปกติ
2. `docker compose -f platform/docker-compose.yml up -d` → บอร์ดต่อ broker เองทันที ไม่ต้องเปิด firewall เพิ่ม (Docker Desktop มีกฎ firewall ของตัวเอง)
3. รัน `py platform/ingest/ingest.py` และ `py platform/dashboard/app.py --port 8080`
4. ตรวจ payload ด้วย `docker exec iotlab-broker mosquitto_sub -t "iotlab/#" -v` → `status online` + telemetry ทุก 10 วิ field ครบตาม contract · `ts` เป็นเวลาจริง → ใน SQLite `ts_source = device`
5. ทดสอบดับ broker: `docker stop iotlab-broker` 31 วิ → `docker start iotlab-broker` แล้วจับเวลาจาก log ของ broker และแถวใหม่ใน SQLite

### พังตรงไหน

| อาการ | สาเหตุจริง | แก้ |
|---|---|---|
| ให้ Claude desktop เปลี่ยนชื่อโฟลเดอร์ socket ที่ค้างแทน แต่ Docker ยังล้มเหมือนเดิม (รอบก่อน ๆ) | แอปเป็นแพ็กเกจ MSIX — การเขียนใต้ `%LOCALAPPDATA%` ถูกเบนไป `%LOCALAPPDATA%\Packages\Claude_*\LocalCache\Local\` (เจอโฟลเดอร์ `.stale-*` ค้างในนั้น 3 อัน) · แท็บ Terminal ในแอปก็โดนเหมือนกัน | เปลี่ยนชื่อจาก PowerShell ที่เปิดจาก Start menu เท่านั้น |

### ผล

ทดสอบดับ broker (เกณฑ์ "ถอดปลั๊ก broker แล้วเสียบใหม่ — ingest ต่อกลับเอง ไม่ต้องรันใหม่"):

| เหตุการณ์ | หลัง `docker start` |
|---|---|
| ingest ต่อกลับ (session เดิม) | 2 วิ |
| บอร์ดต่อกลับ + ประกาศ `online` | 10 วิ (เพดาน backoff 15 วิ) |
| แถวใหม่แถวแรกใน SQLite | 10 วิ |
| seq ที่หาย | 79–81 (3 ข้อความ = 3 รอบวัดระหว่างดับ 31 วิ) — บอร์ดตั้งใจทิ้งระหว่างหลุด · ingest เตือนเอง `node-01 ขาดไป 3 ข้อความ (seq 78 → 82)` |

ไม่ต้องแตะบอร์ด ไม่ต้องรัน ingest ใหม่ → **ผ่าน**

รูป: `../../../_build/shots/2026-11-18-dashboard-node01-first.png` (dashboard ครั้งแรกที่มีข้อมูลจากบอร์ดจริง · ข้อมูลเข้า 14/14 จุด)
