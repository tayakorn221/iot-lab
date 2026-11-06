#include "net.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <time.h>

namespace net {
namespace {

WiFiMulti multi;
const WifiNetwork* nets = nullptr;
size_t netCount = 0;
const char* hostName = "";

bool wasUp = false;
bool reconnected = false;
bool mdnsStarted = false;
bool ntpStarted = false;
bool timeLogged = false;
uint32_t lastTry = 0;
bool triedOnce = false;

// เวลาก่อนนี้ = นาฬิกายังไม่ได้ตั้ง (บอร์ดบูตมาที่ปี 1970)
constexpr time_t VALID_AFTER = 1700000000;  // พ.ย. 2023

// เหตุผลที่หลุด/ต่อไม่ติด — ช่วยแยก "รหัสผิด" กับ "หาเครือข่ายไม่เจอ" กับ "เราเตอร์ดับ"
const char* reasonText(uint8_t r) {
  switch (r) {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "wrong password? (or WPA2-Enterprise -- ESP32 cannot join that)";
    case WIFI_REASON_NO_AP_FOUND:
      return "SSID not in range (2.4 GHz only)";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "AP vanished (router off / out of range)";
    case WIFI_REASON_ASSOC_LEAVE:
      return "left on purpose";
    default:
      return "";
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  // callback นี้รันใน task ของ Wi-Fi ไม่ใช่ loop() — ทำแค่พิมพ์ log
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      uint8_t r = info.wifi_sta_disconnected.reason;
      Serial.printf("[wifi] disconnected, reason %u %s\n", r, reasonText(r));
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[wifi] got IP %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
      break;
    default:
      break;
  }
}

void onUp() {
  Serial.printf("[wifi] up: SSID \"%s\"  IP %s  RSSI %d dBm  gateway %s\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.gatewayIP().toString().c_str());

  if (!mdnsStarted) {
    // ต้องเปิด mDNS ของตัวเองก่อนถึงจะถามชื่อคนอื่นได้ · บอร์ดเองก็จะตอบที่ <device_id>.local
    mdnsStarted = MDNS.begin(hostName);
    Serial.printf("[mdns] %s (%s.local)\n", mdnsStarted ? "started" : "start FAILED", hostName);
  }
  if (!ntpStarted) {
    // ทำครั้งเดียวพอ — SNTP ของ ESP-IDF ซิงก์ซ้ำเองเป็นระยะ ไม่ block
    // UTC ตรง ๆ (offset 0) เพราะ contract ส่ง Unix time · แปลงเวลาไทยเป็นงานของ dashboard
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    ntpStarted = true;
  }
}

}  // namespace

void begin(const char* hostname, const WifiNetwork* networks, size_t count) {
  nets = networks;
  netCount = count;
  hostName = hostname;

  WiFi.persistent(false);       // ไม่เขียนรหัสลง flash ทุกครั้งที่ต่อ (flash สึกได้)
  WiFi.setHostname(hostname);   // ต้องตั้งก่อน WiFi.mode() — ชื่อที่โผล่ในหน้าเราเตอร์
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // ให้ WiFiMulti คุมการต่อใหม่คนเดียว ไม่ชนกับ driver ที่ต่อเอง
  WiFi.onEvent(onWifiEvent);

  for (size_t i = 0; i < count; i++) {
    multi.addAP(networks[i].ssid, networks[i].pass);
  }
  Serial.printf("[wifi] %u known network(s)\n", (unsigned)count);
}

bool loop() {
  bool up = WiFi.status() == WL_CONNECTED;

  if (!up) {
    uint32_t now = millis();
    if (!triedOnce || now - lastTry >= WIFI_RETRY_MS) {
      triedOnce = true;
      if (wasUp) Serial.println("[wifi] lost -- searching");
      wasUp = false;
      // สแกน → เลือก SSID ที่รู้จักและแรงสุด → ต่อ · ค้างได้ถึง ~2 วิ + WIFI_CONNECT_TIMEOUT_MS
      // ระหว่างนี้ไม่มีเน็ตอยู่แล้ว จึงไม่มีอะไรต้องส่ง (ด่าน 3 แยก task ให้การอ่านค่าไม่หยุด)
      up = multi.run(WIFI_CONNECT_TIMEOUT_MS) == WL_CONNECTED;
      lastTry = millis();
      if (!up) Serial.printf("[wifi] no known network joined, retry in %lus\n", WIFI_RETRY_MS / 1000);
    }
  }

  if (up && !wasUp) {
    wasUp = true;
    reconnected = true;
    onUp();
  }

  if (!timeLogged && unixTime() != 0) {
    timeLogged = true;
    Serial.printf("[ntp] time synced: %lu\n", (unsigned long)unixTime());
  }
  return up;
}

bool takeReconnectedFlag() {
  bool r = reconnected;
  reconnected = false;
  return r;
}

const WifiNetwork* current() {
  if (WiFi.status() != WL_CONNECTED) return nullptr;
  String ssid = WiFi.SSID();
  for (size_t i = 0; i < netCount; i++) {
    if (ssid == nets[i].ssid) return &nets[i];
  }
  return nullptr;
}

IPAddress mdnsLookup(const char* host) {
  if (!mdnsStarted || host == nullptr || host[0] == '\0') return IPAddress();
  return MDNS.queryHost(host, MDNS_QUERY_TIMEOUT_MS);
}

uint32_t unixTime() {
  time_t now = time(nullptr);
  return now >= VALID_AFTER ? (uint32_t)now : 0;
}

}  // namespace net
