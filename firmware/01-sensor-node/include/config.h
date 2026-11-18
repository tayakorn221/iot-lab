// config.h — ค่าตั้งที่ไม่ใช่ความลับ (commit ได้) · ความลับอยู่ใน secrets.h
#pragma once

#include <stdint.h>

// ---------- ตัวตนของโหนด ----------
// ตั้งต่อบอร์ดใน platformio.ini ([env:node-01], [env:node-02]) ไม่ต้องแก้ที่นี่
#ifndef DEVICE_ID
#define DEVICE_ID "node-01"
#endif

#define FW_VERSION "0.2.0"

// ---------- จังหวะ ----------
constexpr uint32_t PUBLISH_INTERVAL_MS = 10000;  // ROADMAP ด่าน 1: ทุก 10 วินาที

// ---------- ขา (ESP32 DevKit 30/38 ขาใช้ขาเดียวกัน) ----------
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;
constexpr uint32_t I2C_HZ = 100000;  // 100 kHz พอสำหรับอ่านทุก 10 วินาที และทนสายจัมเปอร์ยาวกว่า 400 kHz
constexpr int LED_PIN = 2;           // ไฟสีฟ้าบน NodeMCU ESP32 / DevKit ส่วนใหญ่

// ---------- เครือข่าย ----------
constexpr uint32_t WIFI_RETRY_MS = 5000;        // ห่างกันอย่างน้อยเท่านี้ระหว่างรอบหา Wi-Fi
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 6000;
constexpr uint32_t WIFI_EAP_CONNECT_TIMEOUT_MS = 15000;  // WPA2-Enterprise คุยกับ RADIUS หลายรอบ ช้ากว่าแบบรหัสเดียวมาก
constexpr uint32_t MDNS_QUERY_TIMEOUT_MS = 1500;
constexpr uint32_t MQTT_RETRY_MIN_MS = 2000;     // ต่อ broker ไม่ติด รอ 2 → 4 → 8 → 15 วินาที
constexpr uint32_t MQTT_RETRY_MAX_MS = 15000;    // เพดานต่ำพอให้ผ่านเกณฑ์ "กลับมาส่งใน 60 วินาที"
constexpr int MQTT_KEEPALIVE_S = 30;             // เท่ากับ fake-node · broker ประกาศ offline หลังเงียบ ~45 วินาที
constexpr int MQTT_COMMAND_TIMEOUT_MS = 2000;    // รอ CONNACK / PUBACK นานสุดเท่านี้

// ทางหนีสุดท้าย: ไม่ได้ต่อ broker นานเท่านี้ → รีบูตตัวเอง (seq เริ่มนับใหม่ — เห็นได้ใน DB)
// ตั้งยาวกว่าการทดสอบด่าน 1 (ปิดเราเตอร์ 5 นาที) มาก เพื่อไม่ให้ไปบังบั๊กการต่อกลับเอง
constexpr uint32_t RESTART_AFTER_OFFLINE_MS = 30UL * 60UL * 1000UL;

// ---------- ชนิดข้อมูลที่ secrets.h ใช้ ----------
struct WifiNetwork {
  const char* ssid;
  const char* pass;
  const char* broker_ip;  // IP สำรองของ broker ในเครือข่ายนี้ ("" = ไม่มี ใช้ mDNS อย่างเดียว)
};

// Wi-Fi แบบ WPA2-Enterprise (ล็อกอินด้วยบัญชี) วิธี PEAP/MSCHAPv2 — เช่น Wi-Fi มหาวิทยาลัย
// identity  = ชื่อด้านนอกที่ส่งก่อนเข้าอุโมงค์เข้ารหัส ("" = ใช้ username)
// ca_pem    = ใบรับรอง CA ของ RADIUS server · nullptr = ไม่ตรวจว่าคุยกับ server ตัวจริง (ดูคำเตือนใน secrets.example.h)
struct WifiEap {
  const char* ssid;
  const char* identity;
  const char* username;
  const char* password;
  const char* ca_pem;
  const char* broker_ip;
};

// ---------- ตรวจ device_id ตาม contract ตอน compile ----------
constexpr bool deviceIdCharsOk(const char* s) {
  return *s == '\0' ? true
                    : (((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-') &&
                       deviceIdCharsOk(s + 1));
}
static_assert(DEVICE_ID[0] != '\0' && deviceIdCharsOk(DEVICE_ID),
              "DEVICE_ID ใช้ได้แค่ a-z 0-9 และ - (shared/mqtt-contract.md)");
