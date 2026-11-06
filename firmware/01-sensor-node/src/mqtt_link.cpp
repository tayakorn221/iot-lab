#include "mqtt_link.h"

#include <Arduino.h>
#include <MQTT.h>
#include <WiFi.h>

#include "config.h"
#include "net.h"

namespace mqtt_link {
namespace {

WiFiClient tcp;
MQTTClient client(512);  // buffer 512 ไบต์ — payload เต็มทุก field ราว 150 ไบต์

const char* id = "";
const char* statusTopic = "";
const char* mdnsHost = "";
uint16_t port = 1883;

IPAddress brokerIp;          // IP ที่ต่อติดล่าสุด · 0.0.0.0 = ต้องหาใหม่
uint32_t nextAttempt = 0;
uint32_t retryDelay = MQTT_RETRY_MIN_MS;
uint32_t failCount = 0;
bool wasConnected = false;
uint32_t lastOnline = 0;      // millis() ครั้งล่าสุดที่เห็นว่ายังต่อ broker อยู่

bool isSet(const IPAddress& ip) { return ip != IPAddress(0, 0, 0, 0); }

// หา IP ของ broker สำหรับความพยายามครั้งนี้
// สลับกันระหว่าง mDNS กับ IP สำรอง — ถ้า mDNS ได้ IP ที่ต่อไม่ติด (เช่นเครื่องมีหลายการ์ดเครือข่าย)
// รอบถัดไปจะได้ลอง IP สำรองบ้าง
IPAddress resolve(const char** source) {
  if (isSet(brokerIp)) {
    *source = "cached";
    return brokerIp;
  }

  const WifiNetwork* w = net::current();
  IPAddress fallback;
  bool hasFallback = w != nullptr && w->broker_ip[0] != '\0' && fallback.fromString(w->broker_ip);
  bool hasMdns = mdnsHost[0] != '\0';
  bool mdnsTurn = hasMdns && (!hasFallback || failCount % 2 == 0);

  if (mdnsTurn) {
    IPAddress ip = net::mdnsLookup(mdnsHost);
    if (isSet(ip)) {
      *source = "mDNS";
      return ip;
    }
    Serial.printf("[mqtt] mDNS: %s.local not found\n", mdnsHost);
  }
  if (hasFallback) {
    *source = "fallback IP";
    return fallback;
  }
  return IPAddress();
}

void tryConnect() {
  const char* source = "";
  IPAddress ip = resolve(&source);
  if (!isSet(ip)) {
    Serial.println("[mqtt] no broker address (mDNS failed, no fallback IP for this Wi-Fi)");
    failCount++;
    return;
  }

  Serial.printf("[mqtt] connecting to %s:%u (%s) ...\n", ip.toString().c_str(), port, source);
  client.setHost(ip, port);
  // connect() ค้างได้ ~3 วิ (TCP timeout ของ WiFiClient) + MQTT_COMMAND_TIMEOUT_MS รอ CONNACK
  if (client.connect(id)) {
    brokerIp = ip;
    failCount = 0;
    retryDelay = MQTT_RETRY_MIN_MS;
    wasConnected = true;
    lastOnline = millis();
    // ประกาศ online แบบ retain — ใครเพิ่ง subscribe ก็เห็นสถานะล่าสุดทันที
    bool ok = client.publish(statusTopic, "online", true, 1);
    Serial.printf("[mqtt] connected · status online %s\n", ok ? "sent" : "FAILED");
    return;
  }

  Serial.printf("[mqtt] connect failed: err %d, return code %d\n", (int)client.lastError(),
                (int)client.returnCode());
  if (net::current() != nullptr && failCount == 0) {
    Serial.println("[mqtt] hint: Wi-Fi OK but broker unreachable -> broker down? firewall on port 1883?"
                   " guest Wi-Fi blocking devices from each other? captive portal?");
  }
  brokerIp = IPAddress();  // IP นี้ใช้ไม่ได้แล้ว รอบหน้าหาใหม่
  failCount++;
  tcp.stop();
}

}  // namespace

void begin(const char* clientId, const char* statusTopic_, const char* mdnsHost_, uint16_t port_) {
  id = clientId;
  statusTopic = statusTopic_;
  mdnsHost = mdnsHost_;
  port = port_;
  lastOnline = millis();  // นับเวลาออฟไลน์จากตอนบูต

  client.begin(tcp);
  // Last Will: ถ้าบอร์ดหายไปเฉย ๆ (ไฟดับ, เน็ตหลุด) broker ประกาศ offline แทนให้
  client.setWill(statusTopic, "offline", true, 1);
  client.setKeepAlive(MQTT_KEEPALIVE_S);
  client.setTimeout(MQTT_COMMAND_TIMEOUT_MS);
  client.setCleanSession(true);
}

void onWifiReconnected() {
  brokerIp = IPAddress();
  failCount = 0;
  retryDelay = MQTT_RETRY_MIN_MS;
  nextAttempt = millis();  // ต่อ broker ทันที — สำคัญกับเกณฑ์ "กลับมาส่งเองใน 60 วินาที"
}

void loop(bool wifiUp) {
  if (!wifiUp) {
    if (wasConnected) {
      // Wi-Fi หาย แต่ TCP ยังคิดว่าต่ออยู่ — ตัดทิ้งเลย ไม่ต้องรอ keepalive หมดเวลา
      // ห้ามใช้ client.disconnect(): มันส่ง DISCONNECT ซึ่งสั่ง broker ไม่ให้ประกาศ Last Will
      Serial.println("[mqtt] Wi-Fi lost -> dropping MQTT session");
      tcp.stop();
      wasConnected = false;
    }
    return;
  }

  if (client.connected()) {
    client.loop();  // ส่ง PINGREQ ตามรอบ keepalive · ตรวจว่า broker ยังอยู่
    if (client.connected()) {
      lastOnline = millis();
      return;
    }
  }

  if (wasConnected) {
    Serial.printf("[mqtt] lost connection (err %d) -- reconnecting\n", (int)client.lastError());
    wasConnected = false;
    tcp.stop();
    nextAttempt = millis();
  }

  uint32_t now = millis();
  if ((int32_t)(now - nextAttempt) < 0) return;

  tryConnect();
  if (!client.connected()) {
    Serial.printf("[mqtt] retry in %lus\n", retryDelay / 1000);
    nextAttempt = millis() + retryDelay;
    retryDelay = min(retryDelay * 2, MQTT_RETRY_MAX_MS);
  }
}

bool connected() { return client.connected(); }

bool publish(const char* topic, const char* payload) {
  if (!client.connected()) return false;
  bool ok = client.publish(topic, payload, false, 1);
  if (!ok) {
    // ไม่ได้ PUBACK = เส้นทางมีปัญหา · ตัดแล้วให้ loop() ต่อใหม่ แทนที่จะรอ keepalive
    Serial.printf("[mqtt] publish failed (err %d) -- dropping connection\n", (int)client.lastError());
    tcp.stop();
  }
  return ok;
}

uint32_t lastOnlineAt() { return lastOnline; }

}  // namespace mqtt_link
