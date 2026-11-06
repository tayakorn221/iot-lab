// env_sensor.h — เซนเซอร์อุณหภูมิ/ความชื้น/ความกด ตระกูล Bosch BME280 บน I2C
//
// ตอนบูตอ่าน chip ID เองก่อน (register 0xD0) เพื่อแยกของจริงกับของปลอม:
//   0x60 = BME280 แท้ → temp_c + rh_pct + pres_hpa
//   0x58 = BMP280 (ร้านส่งผิด/ของปลอม) → temp_c + pres_hpa เท่านั้น ไม่มี rh_pct
// สายหลุดกลางทาง → ไม่ส่ง field ของเซนเซอร์ แล้วลองหาใหม่ทุกรอบ ไม่ต้องรีบูต
#pragma once

#include <Wire.h>

#include "telemetry.h"

class EnvSensor {
 public:
  enum class Kind { None, BME280, BMP280 };

  void begin(TwoWire& wire);
  void read(Telemetry& t);  // เติม field ที่อ่านได้ลง t · อ่านไม่ได้ปล่อยว่าง
  Kind kind() const { return kind_; }

 private:
  bool probe();             // หาเซนเซอร์ที่ 0x76 / 0x77 แล้วเริ่มใช้งาน
  bool present() const;     // ยังตอบ I2C อยู่ไหม
  int readChipId(uint8_t addr) const;

  TwoWire* wire_ = nullptr;
  Kind kind_ = Kind::None;
  uint8_t addr_ = 0;
  bool warnedMissing_ = false;
};
