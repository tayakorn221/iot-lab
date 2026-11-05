"""แปลงข้อความ MQTT เป็นข้อมูลตาม shared/mqtt-contract.md (v1)

ไฟล์นี้ไม่แตะ MQTT และไม่แตะฐานข้อมูล — รับ bytes เข้า คืนข้อมูลที่ตรวจแล้วออก
จึงทดสอบได้โดยไม่ต้องมี broker (ดู test_contract.py)
แก้ contract เมื่อไหร่ ให้แก้ shared/mqtt-contract.md ก่อน แล้วค่อยแก้ไฟล์นี้ตาม
"""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass

TOPIC_RE = re.compile(r"^iotlab/v1/([a-z0-9-]+)/(telemetry|status)$")
METRICS = ("temp_c", "rh_pct", "pres_hpa")
STATUSES = ("online", "offline")


class ContractError(ValueError):
    """ข้อความไม่ตรง contract — ingest ทิ้งข้อความนี้แล้วเขียน log"""


@dataclass(frozen=True)
class Telemetry:
    device_id: str
    ts: int  # Unix วินาที UTC ที่ใช้จริง
    ts_source: str  # "device" = เวลาจากบอร์ด · "ingest" = บอร์ดส่ง 0 มา เลยใช้เวลาที่รับแทน
    seq: int
    temp_c: float | None
    rh_pct: float | None
    pres_hpa: float | None
    raw: str  # payload ดิบ เก็บไว้เผื่อ contract เพิ่ม field ทีหลัง


def parse_topic(topic: str) -> tuple[str, str]:
    """คืน (device_id, kind) โดย kind เป็น "telemetry" หรือ "status" """
    m = TOPIC_RE.match(topic)
    if not m:
        raise ContractError(f"topic ไม่ตรง contract: {topic!r}")
    return m.group(1), m.group(2)


def _is_int(v: object) -> bool:
    # bool เป็นลูกของ int ใน Python — true/false ใน JSON ต้องไม่ผ่านเป็นตัวเลข
    return isinstance(v, int) and not isinstance(v, bool)


def _metric(data: dict, name: str) -> float | None:
    if name not in data:
        return None  # contract: อ่านค่าไม่ได้ให้ไม่ส่ง field นั้น
    v = data[name]
    if not (_is_int(v) or isinstance(v, float)):
        raise ContractError(f"{name} ต้องเป็นตัวเลข ได้ {v!r}")
    if not math.isfinite(v):
        raise ContractError(f"{name} ต้องเป็นตัวเลขจริง ได้ {v!r}")
    return float(v)


def parse_telemetry(device_id: str, payload: bytes, received_at: int) -> Telemetry:
    try:
        raw = payload.decode("utf-8")
        data = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        raise ContractError(f"payload ไม่ใช่ JSON: {e}") from None
    if not isinstance(data, dict):
        raise ContractError("payload ต้องเป็น JSON object")

    for key in ("ts", "seq"):
        if key not in data:
            raise ContractError(f"ขาด field บังคับ {key!r}")
        if not _is_int(data[key]) or data[key] < 0:
            raise ContractError(f"{key} ต้องเป็นจำนวนเต็มไม่ติดลบ ได้ {data[key]!r}")

    if data["ts"] == 0:
        ts, ts_source = received_at, "ingest"
    else:
        ts, ts_source = data["ts"], "device"

    return Telemetry(
        device_id=device_id,
        ts=ts,
        ts_source=ts_source,
        seq=data["seq"],
        temp_c=_metric(data, "temp_c"),
        rh_pct=_metric(data, "rh_pct"),
        pres_hpa=_metric(data, "pres_hpa"),
        raw=raw,
    )


def parse_status(payload: bytes) -> str:
    try:
        s = payload.decode("utf-8").strip()
    except UnicodeDecodeError:
        raise ContractError("status ต้องเป็นข้อความ") from None
    if s not in STATUSES:
        raise ContractError(f"status ต้องเป็น online หรือ offline ได้ {s!r}")
    return s
