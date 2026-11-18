// secrets.example.h — แม่แบบ commit ได้ · ห้ามใส่รหัสจริงในไฟล์นี้
//
// วิธีใช้: ก๊อปไฟล์นี้เป็น include/secrets.h แล้วแก้ค่าในนั้น
//   secrets.h อยู่ใน .gitignore แล้ว — เช็กด้วย `git check-ignore -v include/secrets.h`
#pragma once

#include "config.h"

// Wi-Fi ที่บอร์ดรู้จัก — ใส่ได้หลายชุด บอร์ดสแกนแล้วเลือกตัวที่สัญญาณแรงสุดเอง (รวม Wi-Fi แบบ Enterprise ข้างล่าง)
// ตรงนี้คือ WPA2-Personal (รหัสเดียว) ย่าน 2.4 GHz · Wi-Fi แบบล็อกอินด้วยบัญชีใส่ที่ WIFI_EAP ข้างล่าง
// ใช้ไม่ได้: captive portal (หน้าเว็บให้กดยอมรับ), 5 GHz อย่างเดียว
//
// ช่องที่สาม = IP ของเครื่องที่รัน Mosquitto ในเครือข่ายนั้น ใช้เมื่อหาด้วย mDNS ไม่เจอ
//   ไม่รู้หรือ IP เปลี่ยนบ่อย ใส่ "" ไว้ก็ได้
static const WifiNetwork WIFI_NETWORKS[] = {
    {"home-wifi-ssid", "home-wifi-password", "192.168.1.50"},
    {"phone-hotspot-ssid", "phone-hotspot-password", ""},
};

// ชื่อเครื่องที่รัน Mosquitto (ไม่ต้องมี .local) — Windows ดูได้จากคำสั่ง `hostname`
// บอร์ดจะถาม "<ชื่อนี้>.local" ทาง mDNS ทุกครั้งที่ต้องต่อ broker ใหม่ จึงไม่ต้องตาม IP ที่เปลี่ยน
// ใส่ "" ถ้าจะใช้ IP จาก WIFI_NETWORKS อย่างเดียว
#define BROKER_MDNS_HOST "my-laptop"
#define BROKER_PORT 1883

// ---------- Wi-Fi แบบ WPA2-Enterprise (ล็อกอินด้วยบัญชี เช่น Wi-Fi มหาวิทยาลัย) — ไม่ใช้ก็ตั้ง HAS_WIFI_EAP เป็น 0 ----------
// รองรับเฉพาะ PEAP (ดูได้บน Windows: netsh wlan show profile name="<ชื่อ Wi-Fi>" → EAP type)
//
// คำเตือนก่อนใส่บัญชีจริง:
// - รหัสในไฟล์นี้ไปอยู่ใน flash ของบอร์ดแบบไม่เข้ารหัส (จนถึงด่าน 4 ที่เปิด flash encryption) — บอร์ดหาย = บัญชีหลุด
// - ca_pem = nullptr แปลว่าบอร์ดไม่ตรวจว่าคุยกับ RADIUS server ตัวจริง — Wi-Fi ปลอมชื่อเดียวกันหลอกเอา hash รหัสได้
//   ใส่ใบรับรอง CA ของ server (ข้อความ PEM เต็มก้อน) เพื่อปิดช่องนี้
// - ถามฝ่ายไอทีก่อนใช้จริง: บางที่ห้ามเอาบัญชีส่วนตัวไปผูกกับอุปกรณ์ และบางที่มีวง Wi-Fi สำหรับ IoT ให้แทน
#define HAS_WIFI_EAP 0
static const WifiEap WIFI_EAP = {
    "university-wifi-ssid",  // ssid
    "",                      // identity ภายนอก ("" = ใช้ username)
    "your-username",         // username
    "your-password",         // password
    nullptr,                 // ca_pem
    "",                      // IP สำรองของ broker ในเครือข่ายนี้ ("" = mDNS อย่างเดียว)
};
