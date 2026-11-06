// telemetry.h — ข้อมูลหนึ่งรอบวัด และการแปลงเป็น JSON ตาม shared/mqtt-contract.md (v1)
//
// เซนเซอร์แต่ละตัวเติมเฉพาะ field ของตัวเอง · field ที่ไม่ได้เติม = ไม่ส่ง
// (contract: อ่านไม่ได้ให้ไม่ส่ง ห้ามส่ง 0 — 0 องศาเป็นค่าจริงได้)
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

struct Telemetry {
  uint32_t ts = 0;   // Unix วินาที UTC · 0 = บอร์ดยังไม่ได้เวลาจาก NTP
  uint32_t seq = 0;  // นับจากบูต

  // BME280 (BMP280 ไม่มี rh_pct)
  float temp_c = NAN;
  float rh_pct = NAN;
  float pres_hpa = NAN;

  // PMS5003 — ด่าน 1+ · -1 = ไม่มีค่า
  int pm1_ugm3 = -1;
  int pm25_ugm3 = -1;
  int pm10_ugm3 = -1;
};

// เขียน JSON ลง buf · คืนความยาว หรือ 0 ถ้า buf เล็กไป
size_t telemetryToJson(const Telemetry& t, char* buf, size_t len);
