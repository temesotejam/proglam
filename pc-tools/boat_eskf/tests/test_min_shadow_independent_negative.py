import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from boat_eskf.binlog import MAGIC
from boat_eskf.min_shadow_log import (
    SNAPSHOT, INA, VESC, WAYPOINT_ACK_STRUCT, WAYPOINT_SET_STRUCT,
    decode_min_shadow,
)

HEADER = struct.Struct("<BBHIIQH")


def snapshot(ts=1_000, seq=1):
    values = [0] * 52
    values[0] = ts
    values[1] = seq
    values[3:6] = [0, 0, 0]
    values[6:10] = [35.0, 139.0, 35.0, 139.0]
    values[40:52] = [1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0]
    return SNAPSHOT.pack(*values)


def record(stream, typ, seq, timestamp, payload, version=1):
    stream.write(struct.pack("<IQ", MAGIC, timestamp))
    stream.write(HEADER.pack(version, typ, len(payload), seq, 7, timestamp, 0))
    stream.write(payload)


def ina(timestamp, valid=1):
    return INA.pack(timestamp, 0, 12.0, 0.1, 1.0, 12.0, valid, 0 if valid else 2, 0)


def vesc(timestamp, valid=1, fault=0):
    return VESC.pack(timestamp, 0, 24.0, 2.0, 1.0, 0.1, 1000.0, 40.0,
                     42.0, 10, valid, 1, fault, 0)


class IndependentNegativeDiagnosticsTest(unittest.TestCase):
    def decode(self, build):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "negative.BIN"
            with path.open("wb") as stream:
                build(stream)
            return decode_min_shadow(path)

    def test_future_join_is_rejected_exactly_once(self):
        result = self.decode(lambda f: (record(f, 64, 2, 2_000, ina(2_000)),
                                        record(f, 65, 3, 2_000, vesc(2_000)),
                                        record(f, 63, 1, 1_000, snapshot())))
        self.assertEqual(result["ina_temporal_join_errors"], 1)
        self.assertEqual(result["vesc_temporal_join_errors"], 1)
        self.assertEqual(result["ina_missing_count"], 1)
        self.assertEqual(result["vesc_missing_count"], 1)

    def test_stale_and_invalid_samples_are_counted_exactly(self):
        def build(stream):
            record(stream, 64, 1, 1_000, ina(1))
            record(stream, 65, 2, 1_000, vesc(1))
            record(stream, 63, 3, 600_001, snapshot(600_001, 3))
            record(stream, 64, 4, 2_000, ina(2_000, 0))
            record(stream, 65, 5, 2_000, vesc(2_000, 0))
            record(stream, 63, 6, 2_000, snapshot(2_000, 6))
        result = self.decode(build)
        self.assertEqual(result["ina_stale_count"], 1)
        self.assertEqual(result["vesc_stale_count"], 1)
        self.assertEqual(result["ina_invalid_count"], 1)
        self.assertEqual(result["vesc_invalid_count"], 1)
        self.assertEqual(result["ina_effective_valid_violations"], 0)
        self.assertEqual(result["vesc_effective_valid_violations"], 0)

    def test_vesc_fault_is_effectively_invalid_exactly_once(self):
        def build(stream):
            record(stream, 64, 1, 1_000, ina(1_000))
            record(stream, 65, 2, 1_000, vesc(1_000, 1, 7))
            record(stream, 63, 3, 600_001, snapshot(600_001, 3))
        result = self.decode(build)
        self.assertEqual(result["vesc_fault_count"], 1)
        self.assertEqual(result["vesc_fault_violations"], 0)

    def test_transport_and_waypoint_errors_are_exact(self):
        def build(stream):
            record(stream, 63, 1, 600_001, snapshot(600_001, 1))
            record(stream, 63, 3, 900, snapshot(900, 3))  # one gap and one reversal
            record(stream, 63, 4, 1_100, b"bad")  # known type, wrong payload length
            record(stream, 99, 5, 1_200, b"x", version=2)
            set_payload = bytearray(WAYPOINT_SET_STRUCT.size)
            struct.pack_into("<2I4Bf", set_payload, 0, 1, 1, 1, 0, 0, 0, 0.5)
            struct.pack_into("<I", set_payload, WAYPOINT_SET_STRUCT.size - 4, 0x12345678)
            record(stream, 66, 6, 1_300, bytes(set_payload))
            ack = bytearray(struct.pack("<2I4BI", 1, 1, 3, 0, 0, 0, 0))
            struct.pack_into("<I", ack, WAYPOINT_ACK_STRUCT.size - 4,
                             zlib.crc32(ack[:-4]) & 0xFFFFFFFF)
            record(stream, 67, 7, 1_400, bytes(ack))
        result = self.decode(build)
        self.assertEqual(result["sequence_gaps"], 1)
        self.assertEqual(result["timestamp_reversals"], 1)
        self.assertEqual(result["version_errors"], 1)
        self.assertEqual(result["unknown_types"], 1)
        self.assertEqual(result["payload_length_errors"], 1)
        self.assertEqual(result["waypoint_crc_errors"], 1)
        self.assertEqual(result["waypoint_status_errors"], 1)


if __name__ == "__main__":
    unittest.main()
