#include "net.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wpa2.h>
#include <time.h>

namespace net {
namespace {

const WifiNetwork* nets = nullptr;
size_t netCount = 0;
const WifiEap* eap = nullptr;
WifiNetwork eapAsNet = {"", "", ""};  // ให้ current() คืนชนิดเดียวกับ Wi-Fi แบบรหัสเดียว (mqtt_link ใช้แค่ broker_ip)
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
      return "wrong password? (Enterprise: wrong username/password or server refused this device)";
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

bool hasEap() { return eap != nullptr && eap->ssid != nullptr && eap->ssid[0] != '\0'; }

// สแกนครั้งเดียว → เลือก Wi-Fi ที่รู้จักและสัญญาณแรงสุด (ทั้งแบบรหัสเดียวและแบบ Enterprise) → ต่อ
// แทน WiFiMulti เพราะ WiFiMulti ต่อได้แค่แบบรหัสเดียว
// ค้างได้ถึง ~2 วิสแกน + timeout ของการต่อ — ระหว่างนี้ไม่มีเน็ตอยู่แล้ว (ด่าน 3 แยก task)
bool connectBest() {
  int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.printf("[wifi] scan failed (%d)\n", n);
    return false;
  }

  int bestRssi = -1000;
  int bestPsk = -1;   // index ใน nets
  bool bestIsEap = false;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    int r = WiFi.RSSI(i);
    if (r <= bestRssi) continue;
    for (size_t k = 0; k < netCount; k++) {
      if (s == nets[k].ssid) {
        bestRssi = r;
        bestPsk = (int)k;
        bestIsEap = false;
        break;
      }
    }
    if (hasEap() && s == eap->ssid && r > bestRssi) {
      bestRssi = r;
      bestPsk = -1;
      bestIsEap = true;
    }
  }
  WiFi.scanDelete();

  if (bestPsk < 0 && !bestIsEap) {
    Serial.printf("[wifi] no known network among %d in range\n", n);
    return false;
  }

  uint32_t timeout;
  if (bestIsEap) {
    Serial.printf("[wifi] joining \"%s\" (WPA2-Enterprise PEAP, %d dBm)\n", eap->ssid, bestRssi);
    const char* id = (eap->identity && eap->identity[0]) ? eap->identity : eap->username;
    // ใช้ overload แบบ const char* — แบบ String แปลง nullptr เป็น "" แล้ว core จะตั้ง CA ว่างจนต่อไม่ติด
    WiFi.begin(eap->ssid, WPA2_AUTH_PEAP, id, eap->username, eap->password, eap->ca_pem);
    timeout = WIFI_EAP_CONNECT_TIMEOUT_MS;
  } else {
    const WifiNetwork& w = nets[bestPsk];
    Serial.printf("[wifi] joining \"%s\" (WPA2-Personal, %d dBm)\n", w.ssid, bestRssi);
    // โหมด Enterprise ค้างอยู่ใน driver ถ้าเคยเปิด — ต้องปิดก่อน ไม่งั้นจะเอาบัญชีไปล็อกอิน Wi-Fi รหัสเดียว
    esp_wifi_sta_wpa2_ent_disable();
    WiFi.begin(w.ssid, w.pass);
    timeout = WIFI_CONNECT_TIMEOUT_MS;
  }

  bool ok = WiFi.waitForConnectResult(timeout) == WL_CONNECTED;
  if (!ok) WiFi.disconnect();  // หยุด driver ไม่ให้พยายามค้างต่อ — รอบหน้าสแกนเลือกใหม่
  return ok;
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

void begin(const char* hostname, const WifiNetwork* networks, size_t count, const WifiEap* enterprise) {
  nets = networks;
  netCount = count;
  eap = enterprise;
  hostName = hostname;
  if (hasEap()) eapAsNet = {eap->ssid, "", eap->broker_ip ? eap->broker_ip : ""};

  WiFi.persistent(false);       // ไม่เขียนรหัสลง flash ทุกครั้งที่ต่อ (flash สึกได้)
  WiFi.setHostname(hostname);   // ต้องตั้งก่อน WiFi.mode() — ชื่อที่โผล่ในหน้าเราเตอร์
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // ให้ loop() คุมการต่อใหม่คนเดียว ไม่ชนกับ driver ที่ต่อเอง
  WiFi.onEvent(onWifiEvent);

  Serial.printf("[wifi] %u known network(s)%s\n", (unsigned)count, hasEap() ? " + 1 WPA2-Enterprise" : "");
  if (hasEap() && eap->ca_pem == nullptr) {
    Serial.println("[wifi] WARNING: Enterprise server certificate NOT verified (ca_pem = nullptr)");
  }
}

bool loop() {
  bool up = WiFi.status() == WL_CONNECTED;

  if (!up) {
    uint32_t now = millis();
    if (!triedOnce || now - lastTry >= WIFI_RETRY_MS) {
      triedOnce = true;
      if (wasUp) Serial.println("[wifi] lost -- searching");
      wasUp = false;
      up = connectBest();
      lastTry = millis();
      if (!up) Serial.printf("[wifi] not joined, retry in %lus\n", WIFI_RETRY_MS / 1000);
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
  if (hasEap() && ssid == eap->ssid) return &eapAsNet;
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
