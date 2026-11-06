// secrets.example.h — แม่แบบ commit ได้ · ห้ามใส่รหัสจริงในไฟล์นี้
//
// วิธีใช้: ก๊อปไฟล์นี้เป็น include/secrets.h แล้วแก้ค่าในนั้น
//   secrets.h อยู่ใน .gitignore แล้ว — เช็กด้วย `git check-ignore -v include/secrets.h`
#pragma once

#include "config.h"

// Wi-Fi ที่บอร์ดรู้จัก — ใส่ได้หลายชุด บอร์ดสแกนแล้วเลือกตัวที่สัญญาณแรงสุดเอง
// ESP32 ต่อได้แค่ WPA2-Personal (รหัสเดียว) ย่าน 2.4 GHz
// ใช้ไม่ได้: WPA2-Enterprise (ล็อกอินด้วย user+pass), captive portal (หน้าเว็บให้กดยอมรับ), 5 GHz อย่างเดียว
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
