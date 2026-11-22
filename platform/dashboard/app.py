"""dashboard — อ่าน SQLite ที่ ingest เขียน แล้วเสิร์ฟหน้าเว็บกราฟ

อ่านอย่างเดียว (เปิดฐานแบบ mode=ro) — dashboard พังหรือปิดไป ข้อมูลไม่เสีย ingest เขียนต่อได้
หน้าเว็บอยู่ที่ static/index.html (ไม่พึ่ง CDN วาดกราฟด้วย SVG เอง · ฟอนต์ OFL อยู่ใน static/fonts)

รัน:  py platform/dashboard/app.py                → http://127.0.0.1:8080
      py platform/dashboard/app.py --db <ไฟล์>    → อ่านฐานอื่น เช่น ฐานตัวอย่างจาก tools/seed-demo.py
      py platform/dashboard/app.py --host 0.0.0.0 → ให้เครื่องอื่นในวง LAN เปิดดูได้
"""

from __future__ import annotations

import argparse
import os
import sqlite3
import sys
import time
from pathlib import Path

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

HERE = Path(__file__).resolve().parent
DEFAULT_DB = HERE.parent / "data" / "iotlab.sqlite"
DB_PATH = Path(os.environ.get("DB_PATH", DEFAULT_DB))
# ช่วงส่งที่บอร์ดควรส่ง (ROADMAP ด่าน 1 = ทุก 10 วินาที) ใช้คิด % ความครบของข้อมูล
INTERVAL_S = int(os.environ.get("EXPECTED_INTERVAL_S", "10"))
# ไม่มีข้อมูลเข้านานกว่านี้ ถึงสถานะยัง online ก็ถือว่า "เงียบ"
STALE_S = INTERVAL_S * 6
MAX_POINTS = 720  # จุดต่อเส้นต่อกราฟ — ช่วงยาวจะเฉลี่ยรวมเป็นช่องเวลา
RANGES_H = (1, 6, 24, 168)

app = FastAPI(title="iot-lab dashboard", docs_url=None, redoc_url=None)
# ฟอนต์อยู่ในเครื่อง (static/fonts) — หน้าต้องใช้ได้ตอนไม่มีเน็ต ห้ามดึงจาก CDN
app.mount("/static", StaticFiles(directory=HERE / "static"), name="static")


PM_COLS = ("pm1_ugm3", "pm25_ugm3", "pm10_ugm3")


def connect() -> sqlite3.Connection | None:
    if not DB_PATH.exists():
        return None
    db = sqlite3.connect(f"file:{DB_PATH.as_posix()}?mode=ro", uri=True)
    db.row_factory = sqlite3.Row
    return db


def has_pm(db: sqlite3.Connection) -> bool:
    """ฐานเก่าที่ ingest ยังไม่ได้เปิดเติมคอลัมน์ฝุ่นจะไม่มีคอลัมน์ pm*
    dashboard เปิดฐานแบบอ่านอย่างเดียว เติมเองไม่ได้ — เลยถามก่อนแล้วใช้ NULL แทน"""
    return "pm25_ugm3" in {r[1] for r in db.execute("PRAGMA table_info(telemetry)")}


@app.get("/")
def index():
    return FileResponse(HERE / "static" / "index.html", headers={"Cache-Control": "no-store"})


@app.get("/api/overview")
def overview():
    now = int(time.time())
    db = connect()
    base = {"now": now, "interval_s": INTERVAL_S, "stale_s": STALE_S, "db": str(DB_PATH)}
    if db is None:
        return {**base, "db_found": False, "devices": []}
    with db:
        ids = [r[0] for r in db.execute(
            "SELECT device_id FROM telemetry UNION SELECT device_id FROM device_status ORDER BY 1")]
        devices = []
        since = now - 86400
        pm = has_pm(db)
        for dev in ids:
            last = db.execute(
                "SELECT ts, temp_c, rh_pct, pres_hpa FROM telemetry WHERE device_id = ? ORDER BY ts DESC LIMIT 1",
                (dev,)).fetchone()
            # ค่าฝุ่นล่าสุดแยกถาม — แถวล่าสุดอาจไม่มีฝุ่น (frame checksum ไม่ผ่าน) ทั้งที่โหนดมีเซนเซอร์
            pm_last = db.execute(
                f"SELECT ts, {', '.join(PM_COLS)} FROM telemetry WHERE device_id = ? AND pm25_ugm3 IS NOT NULL "
                "ORDER BY ts DESC LIMIT 1", (dev,)).fetchone() if pm else None
            st = db.execute("SELECT status, updated_at FROM device_status WHERE device_id = ?", (dev,)).fetchone()
            agg = db.execute(
                "SELECT COUNT(*) AS n, MIN(ts) AS first FROM telemetry WHERE device_id = ? AND ts >= ?",
                (dev, since)).fetchone()
            # คาดหวังกี่ข้อความ: นับจากข้อความแรกใน 24 ชม. (โหนดเพิ่งเริ่มจะไม่โดนนับว่าขาดตั้งแต่ 24 ชม. ก่อน)
            expected = ((now - agg["first"]) // INTERVAL_S + 1) if agg["first"] else 0
            devices.append({
                "device_id": dev,
                "status": st["status"] if st else None,
                "status_since": st["updated_at"] if st else None,
                "last": dict(last) if last else None,
                "pm_last": dict(pm_last) if pm_last else None,
                "received_24h": agg["n"],
                "expected_24h": expected,
                "coverage_24h": round(min(1.0, agg["n"] / expected), 4) if expected else None,
            })
    db.close()
    return {**base, "db_found": True, "devices": devices}


@app.get("/api/series")
def series(hours: int = Query(24)):
    if hours not in RANGES_H:
        raise HTTPException(400, f"hours ต้องเป็นหนึ่งใน {RANGES_H}")
    now = int(time.time())
    start = now - hours * 3600
    # ช่องเวลา: อย่างน้อยเท่าช่วงส่งของบอร์ด และไม่เกิน MAX_POINTS จุดต่อเส้น
    bucket = max(INTERVAL_S, -(-hours * 3600 // MAX_POINTS))
    out = {"from": start, "to": now, "bucket_s": bucket, "interval_s": INTERVAL_S, "devices": {}}
    db = connect()
    if db is None:
        return out
    with db:
        pm25 = "pm25_ugm3" if has_pm(db) else "NULL"
        # จุดละ [t, temp, rh, pres, n, pm25] — pm25 ต่อท้าย ไม่ให้ตำแหน่งเดิมเลื่อน
        rows = db.execute(
            "SELECT device_id, (ts / :b) * :b AS t, AVG(temp_c) AS temp_c, AVG(rh_pct) AS rh_pct, "
            f"AVG(pres_hpa) AS pres_hpa, COUNT(*) AS n, AVG({pm25}) AS pm25_ugm3 "
            "FROM telemetry WHERE ts >= :start GROUP BY device_id, t ORDER BY device_id, t",
            {"b": bucket, "start": start})
        for r in rows:
            out["devices"].setdefault(r["device_id"], []).append(
                [r["t"], r["temp_c"], r["rh_pct"], r["pres_hpa"], r["n"], r["pm25_ugm3"]])
    db.close()
    return out


def main() -> None:
    global DB_PATH
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8", errors="replace")
    p = argparse.ArgumentParser(description="iot-lab dashboard")
    p.add_argument("--db", type=Path, default=DB_PATH, help=f"ไฟล์ SQLite (ค่าเริ่ม {DEFAULT_DB})")
    p.add_argument("--host", default="127.0.0.1", help="0.0.0.0 = ให้เครื่องอื่นใน LAN เปิดได้")
    p.add_argument("--port", type=int, default=8080)
    a = p.parse_args()
    DB_PATH = a.db.resolve()
    print(f"อ่านจาก {DB_PATH}{'' if DB_PATH.exists() else '  (ยังไม่มีไฟล์ — เปิด ingest ก่อน)'}", flush=True)
    print(f"เปิด http://{'127.0.0.1' if a.host == '0.0.0.0' else a.host}:{a.port}", flush=True)

    import uvicorn
    uvicorn.run(app, host=a.host, port=a.port, log_level="warning")


if __name__ == "__main__":
    main()
