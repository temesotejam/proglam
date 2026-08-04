"""Decoder and integrity checks for the MIN SHADOW records (types 62..67).

The BIN container is intentionally decoded without assuming a particular run
duration.  CRC/COBS counters are transport diagnostics emitted in the TXT
sidecar; the BIN itself contains the already-decoded frame payload.
"""
from __future__ import annotations

import csv
import math
import struct
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

from .binlog import records

CONTROL_OUTPUT = 62
CONTROL_SNAPSHOT = 63
INA_STATUS = 64
VESC_TELEMETRY = 65
WAYPOINT_SET = 66
WAYPOINT_ACK = 67

SNAPSHOT = struct.Struct("<Q5I4d7f6fH8f8f12B")
INA = struct.Struct("<QI4fBBH")
VESC = struct.Struct("<QI7fi4B")
WAYPOINT_ACK_STRUCT = struct.Struct("<2I4B I")

CSV_FIELDS = (
    "queue_us", "source_us", "sequence", "cycle", "waypoint_revision",
    "latitude_deg", "longitude_deg", "target_waypoint_latitude_deg",
    "target_waypoint_longitude_deg", "speed_mps", "gnss_course_rad",
    "local_north_m", "local_east_m", "target_bearing_rad", "course_error_rad",
    "waypoint_distance_m", "roll_rad", "pitch_rad", "yaw_rad",
    "roll_rate_rad_s", "pitch_rate_rad_s", "yaw_rate_rad_s", "tof_raw_mm",
    "tof_filtered_m", "height_error_m", "u_height", "u_pitch", "u_roll",
    "u_yaw", "front_common", "front_differential", "left_front_wing",
    "right_front_wing", "rear_yaw", "propulsion", "left_prelimit",
    "right_prelimit", "rear_yaw_prelimit", "propulsion_prelimit",
    "gnss_valid", "imu_valid", "tof_valid", "height_valid", "waypoint_reached",
    "output_valid", "state", "safety_reason", "mode", "active_waypoint",
    "ina_valid", "ina_error_code", "bus_voltage_v", "shunt_voltage_v",
    "current_a", "power_w", "vesc_valid", "vesc_mechanical_rpm_valid",
    "input_voltage_v", "motor_current_a", "input_current_a", "duty", "erpm",
    "mos_temp_c", "motor_temp_c", "tachometer",
)


def _finite(values: Iterable[Any]) -> bool:
    return all(not isinstance(v, float) or math.isfinite(v) for v in values)


def _snapshot(payload: bytes) -> dict[str, Any]:
    if len(payload) != SNAPSHOT.size:
        raise ValueError(f"type 63 length {len(payload)} != {SNAPSHOT.size}")
    v = SNAPSHOT.unpack(payload)
    names = ("timestamp_us", "cycle", "waypoint_revision", "gnss_age_us", "imu_age_us", "tof_age_us",
             "latitude_deg", "longitude_deg", "target_waypoint_latitude_deg", "target_waypoint_longitude_deg",
             "speed_mps", "gnss_course_rad", "local_north_m", "local_east_m", "target_bearing_rad",
             "course_error_rad", "waypoint_distance_m", "roll_rad", "pitch_rad", "yaw_rad",
             "roll_rate_rad_s", "pitch_rate_rad_s", "yaw_rate_rad_s", "tof_raw_mm", "tof_filtered_m",
             "height_error_m", "u_height", "u_pitch", "u_roll", "u_yaw", "front_common", "front_differential",
             "left_front_wing", "right_front_wing", "rear_yaw", "propulsion", "left_prelimit", "right_prelimit",
             "rear_yaw_prelimit", "propulsion_prelimit", "gnss_valid", "imu_valid", "tof_valid", "height_valid",
             "waypoint_reached", "output_valid", "state", "safety_reason", "mode", "active_waypoint",
             "reserved0", "reserved1")
    return dict(zip(names, v))


def decode_min_shadow(bin_path: str | Path, csv_path: str | Path | None = None,
                      txt_path: str | Path | None = None) -> dict[str, Any]:
    data = Path(bin_path).read_bytes()
    recs = list(records(data))
    result: dict[str, Any] = {"records": len(recs), "version_errors": 0,
                              "unknown_types": 0, "sequence_gaps": 0,
                              "timestamp_reversals": 0, "nonfinite": 0,
                              "output_range_errors": 0, "csv_rows": 0,
                              "bin_trailing_bytes": 0, "transport_diagnostics": {}}
    snapshots: list[dict[str, Any]] = []
    last_seq: dict[int, int] = {}
    last_ts: dict[int, int] = {}
    ina: dict[str, Any] = {}
    vesc: dict[str, Any] = {}
    known = {CONTROL_OUTPUT, CONTROL_SNAPSHOT, INA_STATUS, VESC_TELEMETRY, WAYPOINT_SET, WAYPOINT_ACK}
    for rec in recs:
        if rec["version"] != 1:
            result["version_errors"] += 1
        typ = rec["type"]
        if typ not in known:
            result["unknown_types"] += 1
        prev = last_seq.get(typ)
        if prev is not None and rec["sequence"] > prev + 1:
            result["sequence_gaps"] += rec["sequence"] - prev - 1
        last_seq[typ] = rec["sequence"]
        prev_t = last_ts.get(typ)
        if prev_t is not None and rec["source_us"] < prev_t:
            result["timestamp_reversals"] += 1
        last_ts[typ] = rec["source_us"]
        if typ == CONTROL_SNAPSHOT:
            row = _snapshot(rec["payload"])
            snapshots.append({**rec, **row})
            floats = [row[k] for k in row if isinstance(row[k], float)]
            if not _finite(floats):
                result["nonfinite"] += 1
            for key in ("left_front_wing", "right_front_wing", "rear_yaw", "left_prelimit", "right_prelimit", "rear_yaw_prelimit"):
                if not -1.0001 <= row[key] <= 1.0001:
                    result["output_range_errors"] += 1
            if not 0.0 <= row["propulsion"] <= 1.0001:
                result["output_range_errors"] += 1
        elif typ == INA_STATUS and len(rec["payload"]) == INA.size:
            t, age, bus, shunt, current, power, valid, error, _ = INA.unpack(rec["payload"])
            ina = {"timestamp_us": t, "age_us": age, "bus_voltage_v": bus, "shunt_voltage_v": shunt,
                   "current_a": current, "power_w": power, "valid": valid, "error_code": error}
        elif typ == VESC_TELEMETRY and len(rec["payload"]) == VESC.size:
            t, age, vin, mc, ic, duty, erpm, mos, mot, tach, valid, rpm_valid, fault, _ = VESC.unpack(rec["payload"])
            vesc = {"timestamp_us": t, "age_us": age, "input_voltage_v": vin, "motor_current_a": mc,
                    "input_current_a": ic, "duty": duty, "erpm": erpm, "mos_temp_c": mos,
                    "motor_temp_c": mot, "tachometer": tach, "valid": valid,
                    "mechanical_rpm_valid": rpm_valid, "fault": fault}
    if csv_path is not None:
        with Path(csv_path).open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
            writer.writeheader()
            for row in snapshots:
                out = {k: row.get(k, "") for k in CSV_FIELDS}
                out.update({"ina_valid": ina.get("valid", ""), "ina_error_code": ina.get("error_code", ""),
                            "bus_voltage_v": ina.get("bus_voltage_v", ""), "shunt_voltage_v": ina.get("shunt_voltage_v", ""),
                            "current_a": ina.get("current_a", ""), "power_w": ina.get("power_w", ""),
                            "vesc_valid": vesc.get("valid", ""), "vesc_mechanical_rpm_valid": vesc.get("mechanical_rpm_valid", ""),
                            "input_voltage_v": vesc.get("input_voltage_v", ""), "motor_current_a": vesc.get("motor_current_a", ""),
                            "input_current_a": vesc.get("input_current_a", ""), "duty": vesc.get("duty", ""),
                            "erpm": vesc.get("erpm", ""), "mos_temp_c": vesc.get("mos_temp_c", ""),
                            "motor_temp_c": vesc.get("motor_temp_c", ""), "tachometer": vesc.get("tachometer", "")})
                writer.writerow(out)
        result["csv_rows"] = len(snapshots)
    if txt_path is not None and Path(txt_path).exists():
        for line in Path(txt_path).read_text(encoding="utf-8", errors="replace").splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                if k.endswith(("crc_errors", "cobs_errors", "length_errors", "queue_drops", "sd_write_errors")):
                    try: result["transport_diagnostics"][k] = int(v)
                    except ValueError: pass
    return result


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("bin")
    p.add_argument("--csv")
    p.add_argument("--txt")
    args = p.parse_args()
    print(decode_min_shadow(args.bin, args.csv, args.txt))
