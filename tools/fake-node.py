"""fake-node — โหนดจำลองที่ทำตัวเหมือน ESP32 ตาม shared/mqtt-contract.md

ใช้ทดสอบ ingest และ dashboard ก่อนบอร์ดจริงมาถึง พฤติกรรมเลียนบอร์ดจริง:
- ตั้ง Last Will = offline (retain) · ต่อติดแล้วประกาศ online (retain)
- seq นับต่อกันตั้งแต่เปิด · ค่าเซนเซอร์เดินแบบสุ่มทีละนิด ไม่กระโดด
- หลุดจาก broker แล้วต่อกลับเอง (ข้อความ QoS 1 ระหว่างหลุดจะถูกพักไว้แล้วส่งตามทีหลัง)

ตัวอย่าง:
  py tools/fake-node.py                          # node fake-01 ส่งทุก 10 วินาที ไม่หยุด
  py tools/fake-node.py --device-id fake-02 --interval 1 --count 30
  py tools/fake-node.py --no-time                # ส่ง ts=0 เหมือนบอร์ดที่ยังไม่ได้เวลา NTP
  py tools/fake-node.py --bad-every 5            # ทุกข้อความที่ 5 ส่ง payload เสีย ไว้ดูว่า ingest ทิ้งถูก
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
import time

import paho.mqtt.client as mqtt


def walk(value: float, step: float, lo: float, hi: float) -> float:
    return min(hi, max(lo, value + random.uniform(-step, step)))


def main() -> None:
    # Windows: ถ้า redirect ลงไฟล์ จะใช้ cp874 ซึ่งพิมพ์ ← ไม่ได้
    # line_buffering: เขียนลงไฟล์ทีละบรรทัด — โปรเซสโดนฆ่าแล้ว log ไม่หาย
    sys.stdout.reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # argparse เขียน error ลงที่นี่
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="localhost")
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--device-id", default="fake-01")
    p.add_argument("--interval", type=float, default=10.0, help="วินาทีระหว่างข้อความ (ค่าเริ่ม 10 ตาม ROADMAP)")
    p.add_argument("--count", type=int, default=0, help="ส่งกี่ข้อความแล้วหยุด (0 = ไม่หยุด)")
    p.add_argument("--no-time", action="store_true", help="ส่ง ts=0 ทุกข้อความ")
    p.add_argument("--bad-every", type=int, default=0, help="ทุกข้อความที่ N ส่ง payload ผิด contract")
    a = p.parse_args()

    if not re.fullmatch(r"[a-z0-9-]+", a.device_id):
        p.error("device-id ใช้ได้แค่ a-z 0-9 และ - (ตาม contract)")

    base = f"iotlab/v1/{a.device_id}"
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=a.device_id)
    client.will_set(f"{base}/status", "offline", qos=1, retain=True)
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    def on_connect(c, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"ต่อ broker ไม่ได้: {reason_code}")
            return
        print(f"[{a.device_id}] ต่อ {a.host}:{a.port} แล้ว")
        c.publish(f"{base}/status", "online", qos=1, retain=True)

    def on_disconnect(c, userdata, flags, reason_code, properties):
        if reason_code != 0:
            print(f"[{a.device_id}] หลุด ({reason_code}) — กำลังต่อใหม่")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.connect_async(a.host, a.port, keepalive=30)
    client.loop_start()

    temp, rh, pres = 27.0, 60.0, 1009.0
    seq = 0
    try:
        while a.count == 0 or seq < a.count:
            seq += 1
            temp = walk(temp, 0.15, 18, 40)
            rh = walk(rh, 0.5, 20, 95)
            pres = walk(pres, 0.1, 990, 1030)
            msg = {
                "ts": 0 if a.no_time else int(time.time()),
                "seq": seq,
                "temp_c": round(temp, 2),
                "rh_pct": round(rh, 1),
                "pres_hpa": round(pres, 1),
            }
            if a.bad_every and seq % a.bad_every == 0:
                msg["temp_c"] = "27.4"  # สตริงแทนตัวเลข — ingest ต้องทิ้ง
                tag = "  ← จงใจผิด"
            else:
                tag = ""
            client.publish(f"{base}/telemetry", json.dumps(msg), qos=1)
            print(f"[{a.device_id}] seq={seq} temp={msg['temp_c']} rh={msg['rh_pct']}{tag}")
            if a.count == 0 or seq < a.count:
                time.sleep(a.interval)
    except KeyboardInterrupt:
        pass
    finally:
        # ปิดแบบตั้งใจ Last Will จะไม่ทำงาน — ต้องประกาศ offline เอง
        client.publish(f"{base}/status", "offline", qos=1, retain=True).wait_for_publish(timeout=3)
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    main()
