// mqtt_link.h — การเชื่อมต่อ MQTT ตาม shared/mqtt-contract.md (v1)
//
// - Last Will = "offline" retain QoS 1 ที่ iotlab/v1/<id>/status
// - ต่อติดแล้วประกาศ "online" retain QoS 1
// - หา broker: mDNS (<BROKER_MDNS_HOST>.local) ก่อน · ไม่เจอใช้ IP สำรองของ Wi-Fi ชุดนั้น
// - หลุดแล้วต่อใหม่เองแบบเว้นระยะ 2 → 4 → 8 → 15 วินาที · ไม่มี while วนรอ
#pragma once

#include <IPAddress.h>
#include <stdint.h>

namespace mqtt_link {

void begin(const char* clientId, const char* statusTopic, const char* mdnsHost, uint16_t port);

// เรียกทุกรอบ loop() · wifiUp = ผลจาก net::loop()
void loop(bool wifiUp);

// Wi-Fi เพิ่งต่อใหม่ — ล้าง IP broker ที่จำไว้ และต่อ broker ทันทีไม่ต้องรอ backoff
void onWifiReconnected();

bool connected();

// QoS 1 ไม่ retain · รอ PUBACK ไม่เกิน MQTT_COMMAND_TIMEOUT_MS · คืน false ถ้าส่งไม่สำเร็จ
bool publish(const char* topic, const char* payload);

// millis() ครั้งล่าสุดที่ยังต่อ broker อยู่ (ก่อนต่อติดครั้งแรก = ตอนบูต)
uint32_t lastOnlineAt();

}  // namespace mqtt_link
