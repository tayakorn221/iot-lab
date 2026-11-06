"""seed-demo — สร้างฐานตัวอย่างที่มีประวัติย้อนหลัง ไว้ดู/พัฒนา dashboard โดยไม่ต้องรอ 24 ชม.

ส่งข้อมูลผ่านโค้ดของ ingest เอง (contract + การเขียนฐาน) ไม่ได้เขียน SQL แยก
ฐานตัวอย่างจึงหน้าตาเหมือนของจริงทุกคอลัมน์

ที่ใส่ไว้ให้เห็นในกราฟ:
- node-01 เดินครบตลอด · อุณหภูมิขึ้นลงตามช่วงวัน · มีเซนเซอร์ฝุ่น PM2.5 พุ่งช่วงรถติดเช้า-เย็น
- node-02 ไม่มีเซนเซอร์ฝุ่น (ไม่ส่ง field pm*) · เริ่มช้ากว่า 6 ชม. · มีช่วงเน็ตหลุด 40 นาที · มีข้อความหายประปราย · ตอนนี้ offline

ต้องระบุ --db เสมอ (กันเขียนทับฐานจริงโดยไม่ตั้งใจ):
  py tools/seed-demo.py --db _demo.sqlite
  py platform/dashboard/app.py --db _demo.sqlite
"""

from __future__ import annotations

import argparse
import json
import logging
import math
import random
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "platform" / "ingest"))
from ingest import DEFAULT_DB_PATH, Ingest, open_db  # noqa: E402


def main() -> None:
    for stream in (sys.stdout, sys.stderr):  # stderr ด้วย — argparse เขียน error ลงที่นั่น
        stream.reconfigure(encoding="utf-8", errors="replace")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--db", type=Path, required=True)
    p.add_argument("--hours", type=int, default=26)
    a = p.parse_args()
    db_path = a.db.resolve()
    if db_path == DEFAULT_DB_PATH.resolve():
        p.error("ห้ามเขียนลงฐานจริง — ใช้ไฟล์อื่น")
    for suffix in ("", "-wal", "-shm"):
        Path(f"{db_path}{suffix}").unlink(missing_ok=True)

    logging.basicConfig(level=logging.ERROR)  # ไม่ต้องการ log ข้อความขาดทีละอัน
    db = open_db(db_path)
    db.execute("PRAGMA synchronous=OFF")  # ฐานตัวอย่าง ไม่ต้องกันไฟดับ — เร็วขึ้นมาก
    ing = Ingest(db)
    random.seed(7)

    now = int(time.time())
    start = now - a.hours * 3600
    n2_start = now - (a.hours - 6) * 3600
    gap = (now - 9 * 3600, now - 9 * 3600 + 40 * 60)  # node-02 หลุด 40 นาที
    n2_end = now - 25 * 60  # node-02 ดับเมื่อ 25 นาทีก่อน

    ing.handle("iotlab/v1/node-01/status", b"online", start)
    ing.handle("iotlab/v1/node-02/status", b"online", n2_start)
    seq = {"node-01": 0, "node-02": 0}
    noise = {"node-01": 0.0, "node-02": 0.0}
    count = 0
    for ts in range(start, now + 1, 10):
        day = math.sin((ts % 86400) / 86400 * 2 * math.pi - 2.2)  # ร้อนสุดช่วงบ่าย (UTC+7)
        for dev, base_t, base_rh in (("node-01", 27.5, 58.0), ("node-02", 25.5, 64.0)):
            if dev == "node-02" and not (n2_start <= ts <= n2_end):
                continue
            seq[dev] += 1
            if dev == "node-02" and (gap[0] <= ts < gap[1] or random.random() < 0.01):
                continue  # หายระหว่างทาง: seq เดินแต่ไม่ถึง ingest
            noise[dev] = noise[dev] * 0.97 + random.uniform(-0.05, 0.05)
            msg = {
                "ts": ts, "seq": seq[dev],
                "temp_c": round(base_t + 2.2 * day + noise[dev] * 4, 2),
                "rh_pct": round(base_rh - 7 * day + noise[dev] * 10, 1),
                "pres_hpa": round(1009 + 1.4 * math.sin(ts / 43200 * 2 * math.pi) + noise[dev], 1),
            }
            if dev == "node-01":
                hour = ((ts + 7 * 3600) % 86400) / 3600  # เวลาไทย
                rush = 22 * math.exp(-((hour - 8) ** 2) / 2) + 16 * math.exp(-((hour - 18.5) ** 2) / 3)
                pm25 = max(1.0, 16 + rush + noise[dev] * 25)
                msg.update(pm1_ugm3=round(pm25 * 0.7), pm25_ugm3=round(pm25), pm10_ugm3=round(pm25 * 1.3))
            ing.handle(f"iotlab/v1/{dev}/telemetry", json.dumps(msg).encode(), ts)
            count += 1
        if ts == gap[0]:
            ing.handle("iotlab/v1/node-02/status", b"offline", ts)
        if ts == gap[1]:
            ing.handle("iotlab/v1/node-02/status", b"online", ts)
    ing.handle("iotlab/v1/node-02/status", b"offline", n2_end)
    db.close()
    print(f"เขียน {count:,} แถวลง {db_path}")


if __name__ == "__main__":
    main()
