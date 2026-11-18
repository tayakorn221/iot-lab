// net.h — Wi-Fi หลายชุด (แบบรหัสเดียว + แบบ Enterprise ได้ 1 ชุด) + mDNS + เวลา NTP
//
// ไม่มี while วนรอเน็ต — loop() เรียกได้ทุกรอบ:
//   ต่ออยู่แล้ว → คืนทันที
//   หลุด → ลองหา Wi-Fi ใหม่ทุก WIFI_RETRY_MS (รอบหนึ่งค้างได้ถึง ~2 วิสแกน + WIFI_CONNECT_TIMEOUT_MS)
#pragma once

#include <IPAddress.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

namespace net {

// enterprise = nullptr ถ้าไม่มี Wi-Fi แบบล็อกอินด้วยบัญชี
void begin(const char* hostname, const WifiNetwork* networks, size_t count, const WifiEap* enterprise);

// คืน true ถ้า Wi-Fi ต่ออยู่และได้ IP แล้ว
bool loop();

// เพิ่งต่อ Wi-Fi ติดใหม่ตั้งแต่เรียกครั้งก่อน (ใช้ล้าง IP broker ที่จำไว้ — อาจย้ายเครือข่ายแล้ว)
bool takeReconnectedFlag();

// Wi-Fi ชุดที่ต่ออยู่ตอนนี้ (nullptr ถ้าไม่ได้ต่อ)
const WifiNetwork* current();

// ถาม <host>.local ทาง mDNS · ไม่เจอคืน 0.0.0.0
IPAddress mdnsLookup(const char* host);

// Unix วินาที UTC · ยังไม่ได้เวลาจาก NTP คืน 0
uint32_t unixTime();

}  // namespace net
