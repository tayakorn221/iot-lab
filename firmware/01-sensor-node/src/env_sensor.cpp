#include "env_sensor.h"

#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Arduino.h>

namespace {

constexpr uint8_t ADDRS[] = {0x76, 0x77};  // ขา SDO ลง GND = 0x76 · ขึ้น VCC = 0x77
constexpr uint8_t REG_CHIP_ID = 0xD0;
constexpr int CHIP_BME280 = 0x60;
constexpr int CHIP_BMP280 = 0x58;

Adafruit_BME280 bme;
Adafruit_BMP280* bmp = nullptr;  // สร้างตอนรู้ว่าเป็น BMP280 — ต้องผูกกับ Wire ที่ส่งเข้ามา

// ช่วงที่ datasheet รับประกัน — นอกนี้ถือว่าอ่านพัง (เช่นสายหลวมจนได้ 0xFF ทั้งก้อน)
bool tempOk(float v) { return isfinite(v) && v >= -40.0f && v <= 85.0f; }
bool rhOk(float v) { return isfinite(v) && v >= 0.0f && v <= 100.0f; }
bool presOk(float v) { return isfinite(v) && v >= 300.0f && v <= 1100.0f; }

}  // namespace

void EnvSensor::begin(TwoWire& wire) {
  wire_ = &wire;
  if (!probe()) {
    Serial.println("[sensor] no BME280 at 0x76/0x77 -- check 4 wires (VIN=3V3 GND SDA=21 SCL=22)");
    warnedMissing_ = true;
  }
}

int EnvSensor::readChipId(uint8_t addr) const {
  wire_->beginTransmission(addr);
  wire_->write(REG_CHIP_ID);
  if (wire_->endTransmission(false) != 0) return -1;  // ไม่มีใครตอบที่ address นี้
  if (wire_->requestFrom(addr, (uint8_t)1) != 1) return -1;
  return wire_->read();
}

bool EnvSensor::present() const {
  wire_->beginTransmission(addr_);
  return wire_->endTransmission() == 0;
}

bool EnvSensor::probe() {
  kind_ = Kind::None;
  for (uint8_t addr : ADDRS) {
    int id = readChipId(addr);
    if (id < 0) continue;

    Serial.printf("[sensor] I2C 0x%02X chip ID = 0x%02X\n", addr, id);

    if (id == CHIP_BME280) {
      if (!bme.begin(addr, wire_)) {
        Serial.println("[sensor] BME280 found but init failed");
        continue;
      }
      // forced mode: วัดเฉพาะตอนสั่ง แล้วหลับ — เซนเซอร์ไม่อุ่นตัวเองจนอุณหภูมิเพี้ยน
      // oversampling x1 + ไม่กรอง = ค่าที่ Bosch แนะนำสำหรับงานวัดสภาพอากาศ (datasheet 3.5.1)
      bme.setSampling(Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::FILTER_OFF);
      kind_ = Kind::BME280;
      addr_ = addr;
      Serial.printf("[sensor] OK: genuine BME280 at 0x%02X (temp + humidity + pressure)\n", addr);
      return true;
    }

    if (id == CHIP_BMP280) {
      Serial.println("[sensor] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("[sensor] !!! chip ID 0x58 = BMP280, NOT BME280 (fake / wrong part)   !!!");
      Serial.println("[sensor] !!! no humidity sensor: rh_pct will NOT be sent            !!!");
      Serial.println("[sensor] !!! still sending temp_c + pres_hpa                         !!!");
      Serial.println("[sensor] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      if (bmp == nullptr) bmp = new Adafruit_BMP280(wire_);
      if (!bmp->begin(addr, CHIP_BMP280)) {
        Serial.println("[sensor] BMP280 init failed");
        continue;
      }
      bmp->setSampling(Adafruit_BMP280::MODE_FORCED, Adafruit_BMP280::SAMPLING_X1,
                       Adafruit_BMP280::SAMPLING_X1, Adafruit_BMP280::FILTER_OFF);
      kind_ = Kind::BMP280;
      addr_ = addr;
      return true;
    }

    Serial.printf("[sensor] unknown chip ID 0x%02X at 0x%02X -- not a BME280 (0x60) or BMP280 (0x58)\n",
                  id, addr);
  }
  return false;
}

void EnvSensor::read(Telemetry& t) {
  if (wire_ == nullptr) return;

  if (kind_ != Kind::None && !present()) {
    Serial.println("[sensor] lost (no I2C ACK) -- will keep looking, sending without sensor fields");
    kind_ = Kind::None;
  }
  if (kind_ == Kind::None) {
    if (!probe()) {
      if (!warnedMissing_) {
        Serial.println("[sensor] still missing");
        warnedMissing_ = true;
      }
      return;
    }
    warnedMissing_ = false;
  }

  float temp = NAN, rh = NAN, pres = NAN;
  if (kind_ == Kind::BME280) {
    if (!bme.takeForcedMeasurement()) {
      Serial.println("[sensor] BME280 measurement timeout");
      return;
    }
    temp = bme.readTemperature();
    rh = bme.readHumidity();
    pres = bme.readPressure() / 100.0f;  // Pa → hPa
  } else {
    if (!bmp->takeForcedMeasurement()) {
      Serial.println("[sensor] BMP280 measurement timeout");
      return;
    }
    temp = bmp->readTemperature();
    pres = bmp->readPressure() / 100.0f;
  }

  if (tempOk(temp)) t.temp_c = temp;
  if (rhOk(rh)) t.rh_pct = rh;
  if (presOk(pres)) t.pres_hpa = pres;
}
