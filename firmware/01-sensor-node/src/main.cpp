// 01-sensor-node — ด่าน 1: ESP32 + BME280 → MQTT ทุก 10 วินาที
// topic/payload ตาม shared/mqtt-contract.md (v1) · พฤติกรรมเลียน tools/fake-node.py
//
// loop() ไม่มี while วนรอ: ทุกรอบ ดูแล Wi-Fi → ดูแล MQTT → ถึงเวลาก็วัดแล้วส่ง → อัปเดตไฟ
// เน็ตหลุด/broker ดับ ระหว่างนั้นยังวัดและนับ seq ต่อ แต่ทิ้งข้อความ (seq ที่ขาดใน DB = ช่วงที่หลุด)
// ด่าน 3 ค่อยเก็บค้างไว้ส่งทีหลัง
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include "config.h"
#include "env_sensor.h"
#include "mqtt_link.h"
#include "net.h"
#include "telemetry.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found -- building with secrets.example.h (cannot join Wi-Fi). Copy it to secrets.h before flashing."
#include "secrets.example.h"
#endif

namespace {

constexpr char TOPIC_TELEMETRY[] = "iotlab/v1/" DEVICE_ID "/telemetry";
constexpr char TOPIC_STATUS[] = "iotlab/v1/" DEVICE_ID "/status";

EnvSensor envSensor;
// ด่าน 1+: PMS5003 จะมาเป็นเซนเซอร์ตัวที่สองตรงนี้ (เช่น DustSensor dust;) แล้วเติม pm*_ugm3 ใน measureAndPublish()

uint32_t seq = 0;
uint32_t nextMeasureAt = 0;
uint32_t ledFlashUntil = 0;

const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "CRASH (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT -- power supply too weak / bad USB cable";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_EXT: return "reset pin";
    default: return "other";
  }
}

void measureAndPublish() {
  Telemetry t;
  t.seq = ++seq;
  t.ts = net::unixTime();
  envSensor.read(t);
  // dust.read(t);  ← ด่าน 1+

  char json[256];
  size_t n = telemetryToJson(t, json, sizeof(json));
  if (n == 0) {
    Serial.println("[pub] JSON buffer too small -- BUG");
    return;
  }

  if (!mqtt_link::connected()) {
    Serial.printf("[pub] offline, dropped seq=%lu %s\n", (unsigned long)t.seq, json);
    return;
  }
  bool ok = mqtt_link::publish(TOPIC_TELEMETRY, json);
  Serial.printf("[pub] %s %s\n", ok ? "ok    " : "FAILED", json);
  if (ok) ledFlashUntil = millis() + 80;
}

// ไฟบนบอร์ดบอกสถานะ — ไม่ต้องต่อ serial ก็รู้ว่าค้างตรงไหน
//   กะพริบเร็ว (5 ครั้ง/วิ) = หา Wi-Fi
//   กะพริบช้า (1 ครั้ง/วิ)  = ได้ Wi-Fi แล้ว แต่ต่อ broker ไม่ติด
//   ติดค้าง ดับวูบทุก 10 วิ = ส่งข้อมูลปกติ (ดับวูบ = ส่งสำเร็จหนึ่งข้อความ)
void updateLed(bool wifiUp, bool mqttUp) {
  uint32_t now = millis();
  bool on;
  if (!wifiUp) {
    on = (now / 100) % 2;
  } else if (!mqttUp) {
    on = (now / 500) % 2;
  } else {
    on = (int32_t)(now - ledFlashUntil) >= 0;
  }
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);  // ให้ serial monitor ทันจับบรรทัดแรก
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println("========================================");
  Serial.printf("01-sensor-node v%s  device_id=%s\n", FW_VERSION, DEVICE_ID);
  Serial.printf("built %s %s · reset reason: %s\n", __DATE__, __TIME__, resetReasonText(esp_reset_reason()));
  Serial.printf("telemetry -> %s every %lus\n", TOPIC_TELEMETRY, PUBLISH_INTERVAL_MS / 1000);
  Serial.println("========================================");

  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  envSensor.begin(Wire);

  net::begin(DEVICE_ID, WIFI_NETWORKS, sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]));
  mqtt_link::begin(DEVICE_ID, TOPIC_STATUS, BROKER_MDNS_HOST, BROKER_PORT);

  nextMeasureAt = millis() + 2000;  // ให้เซนเซอร์ตื่นเต็มที่ก่อนวัดครั้งแรก
}

void loop() {
  bool wifiUp = net::loop();
  if (net::takeReconnectedFlag()) mqtt_link::onWifiReconnected();
  mqtt_link::loop(wifiUp);

  uint32_t now = millis();
  if ((int32_t)(now - nextMeasureAt) >= 0) {
    // นับรอบถัดไปจากกำหนดเดิม ไม่ใช่จากตอนนี้ — จังหวะไม่ไหลช้าลงทีละนิด
    nextMeasureAt += PUBLISH_INTERVAL_MS;
    // ถ้าเพิ่งค้างนานเกินรอบ (เช่นหา Wi-Fi) ไม่ต้องส่งรัวชดเชย เริ่มนับใหม่จากตอนนี้
    if ((int32_t)(now - nextMeasureAt) >= 0) nextMeasureAt = now + PUBLISH_INTERVAL_MS;
    measureAndPublish();
  }

  updateLed(wifiUp, mqtt_link::connected());

  if (millis() - mqtt_link::lastOnlineAt() > RESTART_AFTER_OFFLINE_MS) {
    Serial.printf("[main] no broker for %lu min -- restarting as last resort\n",
                  RESTART_AFTER_OFFLINE_MS / 60000);
    delay(100);
    ESP.restart();
  }

  delay(10);  // ให้ task อื่น (Wi-Fi, idle/watchdog) ได้ CPU บ้าง
}
