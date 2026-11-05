"""ingest — รับข้อความจาก broker แล้วเขียนลง SQLite

ฟัง  iotlab/v1/+/telemetry  และ  iotlab/v1/+/status
ตรวจ payload ตาม contract.py → ไม่ผ่านทิ้งแล้วเขียน log · ผ่านเขียนลงฐาน

ความทนทาน (เกณฑ์ผ่านด่าน 1):
- broker ดับ → ต่อกลับเองทุก 1–30 วินาที ไม่ต้องรันใหม่
- ingest ดับ → ใช้ persistent session (clean_session=False + QoS 1)
  broker เก็บข้อความค้างไว้ให้ ต่อกลับมาแล้วได้ของช่วงที่หายไปครบ
- ข้อความซ้ำจาก QoS 1 → กันด้วย unique index (device_id, ts, seq)

ตั้งค่าผ่าน env: MQTT_HOST · MQTT_PORT · DB_PATH
"""

from __future__ import annotations

import logging
import os
import sqlite3
import sys
import time
from pathlib import Path

import paho.mqtt.client as mqtt

from contract import ContractError, Telemetry, parse_status, parse_telemetry, parse_topic

HOST = os.environ.get("MQTT_HOST", "localhost")
PORT = int(os.environ.get("MQTT_PORT", "1883"))
DEFAULT_DB_PATH = Path(__file__).resolve().parent.parent / "data" / "iotlab.sqlite"
DB_PATH = Path(os.environ.get("DB_PATH", DEFAULT_DB_PATH))
CLIENT_ID = "iotlab-ingest"  # ต้องคงที่ broker ถึงจำ session เดิมได้

SCHEMA = """
CREATE TABLE IF NOT EXISTS telemetry (
    id          INTEGER PRIMARY KEY,
    device_id   TEXT    NOT NULL,
    ts          INTEGER NOT NULL,   -- Unix วินาที UTC
    ts_source   TEXT    NOT NULL,   -- device | ingest
    received_at INTEGER NOT NULL,
    seq         INTEGER NOT NULL,
    temp_c      REAL,
    rh_pct      REAL,
    pres_hpa    REAL,
    raw         TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_telemetry_device_ts ON telemetry (device_id, ts);
-- QoS 1 อาจส่งซ้ำ · กันซ้ำเฉพาะแถวที่ใช้เวลาจากบอร์ด (เวลาที่ ingest เติมให้ไม่คงที่ จึงใช้กันซ้ำไม่ได้)
CREATE UNIQUE INDEX IF NOT EXISTS uq_telemetry_dedupe
    ON telemetry (device_id, ts, seq) WHERE ts_source = 'device';

CREATE TABLE IF NOT EXISTS device_status (
    device_id  TEXT PRIMARY KEY,
    status     TEXT    NOT NULL,   -- online | offline
    updated_at INTEGER NOT NULL    -- เวลาที่สถานะเปลี่ยนครั้งล่าสุด
);
"""

log = logging.getLogger("ingest")


def open_db(path: Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(path)
    db.execute("PRAGMA journal_mode=WAL")  # ให้ dashboard อ่านพร้อมกับที่ ingest เขียนได้
    db.executescript(SCHEMA)
    return db


class Ingest:
    def __init__(self, db: sqlite3.Connection):
        self.db = db
        self.last_seq: dict[str, int] = {}

    # --- MQTT callbacks (paho v2) ---

    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            log.error("ต่อ broker ไม่ได้: %s", reason_code)
            return
        resumed = "ต่อ session เดิม" if flags.session_present else "session ใหม่"
        log.info("ต่อ broker %s:%s แล้ว (%s)", HOST, PORT, resumed)
        # subscribe ซ้ำทุกครั้งที่ต่อติด — ถ้า broker ลืม session ไป จะได้ไม่เงียบหาย
        client.subscribe([("iotlab/v1/+/telemetry", 1), ("iotlab/v1/+/status", 1)])

    def on_disconnect(self, client, userdata, flags, reason_code, properties):
        if reason_code != 0:
            log.warning("หลุดจาก broker (%s) — กำลังต่อใหม่", reason_code)

    def on_message(self, client, userdata, msg):
        if not msg.payload:
            # payload ว่าง = มีคนล้างข้อความ retain (mosquitto_pub -r -n) ไม่ใช่ข้อมูล — ข้ามเงียบ ๆ
            return
        try:
            self.handle(msg.topic, msg.payload, int(time.time()))
        except ContractError as e:
            log.warning("ทิ้ง %s: %s · payload=%r", msg.topic, e, msg.payload[:200])
        except Exception:
            # อย่าให้ข้อความเดียวทำ ingest ล้มทั้งตัว
            log.exception("error ไม่คาดคิดตอนจัดการ %s", msg.topic)

    # --- logic ---

    def handle(self, topic: str, payload: bytes, received_at: int) -> None:
        device_id, kind = parse_topic(topic)
        if kind == "status":
            self.save_status(device_id, parse_status(payload), received_at)
        else:
            self.save_telemetry(parse_telemetry(device_id, payload, received_at), received_at)

    def save_status(self, device_id: str, status: str, received_at: int) -> None:
        # status เป็น retain — broker ส่งค่าเดิมซ้ำทุกครั้งที่ ingest ต่อกลับ
        # อัปเดตเฉพาะตอนสถานะเปลี่ยนจริง ไม่งั้น updated_at จะเลื่อนทั้งที่ไม่มีอะไรเกิดขึ้น
        cur = self.db.execute(
            "INSERT INTO device_status (device_id, status, updated_at) VALUES (?, ?, ?) "
            "ON CONFLICT (device_id) DO UPDATE SET status = excluded.status, updated_at = excluded.updated_at "
            "WHERE device_status.status != excluded.status",
            (device_id, status, received_at),
        )
        self.db.commit()
        if cur.rowcount:
            log.info("%s → %s", device_id, status)

    def save_telemetry(self, t: Telemetry, received_at: int) -> None:
        cur = self.db.execute(
            "INSERT OR IGNORE INTO telemetry "
            "(device_id, ts, ts_source, received_at, seq, temp_c, rh_pct, pres_hpa, raw) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            (t.device_id, t.ts, t.ts_source, received_at, t.seq, t.temp_c, t.rh_pct, t.pres_hpa, t.raw),
        )
        self.db.commit()
        if cur.rowcount == 0:
            log.debug("ซ้ำ ข้าม: %s seq=%s", t.device_id, t.seq)
            return
        self.check_seq(t.device_id, t.seq)
        log.debug("%s seq=%s temp=%s rh=%s", t.device_id, t.seq, t.temp_c, t.rh_pct)

    def check_seq(self, device_id: str, seq: int) -> None:
        last = self.last_seq.get(device_id)
        self.last_seq[device_id] = seq
        if last is None:
            return
        if seq > last + 1:
            # อาจหายระหว่างทาง หรือ ingest ทิ้งเองเพราะผิด contract (ดู WARNING "ทิ้ง" ก่อนหน้า)
            log.warning("%s ขาดไป %d ข้อความ (seq %d → %d) — หายระหว่างทาง หรือถูกทิ้งเพราะผิด contract",
                        device_id, seq - last - 1, last, seq)
        elif seq <= last:
            log.info("%s seq เริ่มใหม่ (%d → %d) — บอร์ดน่าจะรีบูต", device_id, last, seq)


def main() -> None:
    # Windows: ถ้า redirect log ลงไฟล์ จะใช้ cp874 ซึ่งพิมพ์ → ไม่ได้ แล้ว log พัง
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8", errors="replace")
    logging.basicConfig(
        level=os.environ.get("LOG_LEVEL", "INFO"),
        format="%(asctime)s %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )
    db = open_db(DB_PATH)
    log.info("เขียนลง %s", DB_PATH)
    ing = Ingest(db)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID, clean_session=False)
    client.on_connect = ing.on_connect
    client.on_disconnect = ing.on_disconnect
    client.on_message = ing.on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.connect_async(HOST, PORT, keepalive=30)
    try:
        # retry_first_connection: เปิด ingest ก่อน broker ก็ได้ มันจะรอจน broker ขึ้น
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        log.info("หยุด")
    finally:
        client.disconnect()
        db.close()


if __name__ == "__main__":
    main()
