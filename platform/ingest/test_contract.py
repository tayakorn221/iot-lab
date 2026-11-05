import pytest

from contract import ContractError, parse_status, parse_telemetry, parse_topic

NOW = 1_793_900_000


def tel(payload: str):
    return parse_telemetry("node-01", payload.encode(), NOW)


# --- topic ---

def test_topic_telemetry():
    assert parse_topic("iotlab/v1/node-01/telemetry") == ("node-01", "telemetry")


def test_topic_status():
    assert parse_topic("iotlab/v1/stg-room-a/status") == ("stg-room-a", "status")


@pytest.mark.parametrize("topic", [
    "iotlab/v1/Node-01/telemetry",  # ตัวพิมพ์ใหญ่
    "iotlab/v1/node_01/telemetry",  # ขีดล่าง
    "iotlab/v2/node-01/telemetry",  # เวอร์ชันอื่น
    "iotlab/v1/node-01/telemetry/x",
    "iotlab/v1//telemetry",
])
def test_topic_rejected(topic):
    with pytest.raises(ContractError):
        parse_topic(topic)


# --- telemetry ---

def test_full_payload():
    t = tel('{"ts":1793865600,"seq":42,"temp_c":27.41,"rh_pct":61.2,"pres_hpa":1009.8}')
    assert (t.ts, t.ts_source, t.seq) == (1793865600, "device", 42)
    assert (t.temp_c, t.rh_pct, t.pres_hpa) == (27.41, 61.2, 1009.8)


def test_ts_zero_uses_received_time():
    t = tel('{"ts":0,"seq":1,"temp_c":27}')
    assert (t.ts, t.ts_source) == (NOW, "ingest")


def test_missing_metric_is_none_not_zero():
    t = tel('{"ts":0,"seq":1,"temp_c":0}')
    assert t.temp_c == 0.0  # 0 องศาเป็นค่าจริง
    assert t.rh_pct is None


def test_int_metric_becomes_float():
    assert tel('{"ts":0,"seq":1,"temp_c":27}').temp_c == 27.0


def test_extra_field_kept_in_raw():
    t = tel('{"ts":0,"seq":1,"co2_ppm":410}')
    assert "co2_ppm" in t.raw


@pytest.mark.parametrize("payload", [
    'not json',
    '[1,2,3]',
    '{"seq":1}',                      # ขาด ts
    '{"ts":0}',                       # ขาด seq
    '{"ts":-5,"seq":1}',
    '{"ts":0,"seq":1.5}',
    '{"ts":true,"seq":1}',            # bool ไม่ใช่ตัวเลข
    '{"ts":0,"seq":1,"temp_c":"27"}',
    '{"ts":0,"seq":1,"temp_c":false}',
    '{"ts":0,"seq":1,"temp_c":NaN}',
])
def test_telemetry_rejected(payload):
    with pytest.raises(ContractError):
        tel(payload)


def test_non_utf8_rejected():
    with pytest.raises(ContractError):
        parse_telemetry("node-01", b"\xff\xfe", NOW)


# --- status ---

@pytest.mark.parametrize("payload,expected", [(b"online", "online"), (b"offline\n", "offline")])
def test_status(payload, expected):
    assert parse_status(payload) == expected


def test_status_rejected():
    with pytest.raises(ContractError):
        parse_status(b"ONLINE")
