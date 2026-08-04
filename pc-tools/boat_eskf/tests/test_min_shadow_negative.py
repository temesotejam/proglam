import os
import sys
import struct
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from boat_eskf.binlog import MAGIC
from boat_eskf.min_shadow_log import (
    SNAPSHOT,
    INA,
    VESC,
    WAYPOINT_ACK_STRUCT,
    decode_min_shadow,
)
HEADER = struct.Struct("<BBHIIQH")

def snap(ts, seq):
    values = [0] * 52
    values[0] = ts
    values[1] = seq
    values[2] = 1
    values[3] = 900_000
    values[4] = 200_000
    values[5] = 300_000
    values[6] = 35.0
    values[7] = 139.0
    values[8] = 35.0
    values[9] = 139.0
    values[40:52] = [1, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0]
    return SNAPSHOT.pack(*values)
def rec(out, typ, seq, ts, payload, version=1):
    out.write(struct.pack("<IQ", MAGIC, ts))
    out.write(HEADER.pack(version, typ, len(payload), seq, 7, ts, 0))
    out.write(payload)

class MinShadowNegativeDiagnosticsTest(unittest.TestCase):
    def test_malformed_fixture_increments_each_safety_counter(self):
        import math
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "negative.BIN"
            with path.open("wb") as f:
                # A stale INA/VESC sample is joined to the first control row.
                rec(f, 64, 1, 1_000, INA.pack(1_000, 0, 12.0, 0.1, 1.0, 12.0, 1, 0, 0))
                rec(f, 65, 1, 1_000, VESC.pack(1_000, 0, 24.0, 2.0, 1.0, 0.1, 1000.0, 40.0, 42.0, 10, 1, 1, 0, 0))
                p = bytearray(snap(1_000_000, 1))
                struct.pack_into("<f", p, 158, 2.0)  # propulsion outside range
                p[184] = 2  # ARMED_IDLE with nonzero output is unsafe
                p[185] = 0  # missing safety reason
                rec(f, 63, 1, 1_000_000, bytes(p))
                # Running output jump and an implicit restart after STOP.
                p2 = bytearray(snap(1_001_000, 2)); p2[184] = 1; struct.pack_into("<f", p2, 146, 0.0)
                rec(f, 63, 2, 1_001_000, bytes(p2))
                p3 = bytearray(snap(1_002_000, 3)); p3[184] = 1; struct.pack_into("<f", p3, 146, 1.0)
                rec(f, 63, 3, 1_002_000, bytes(p3))
                p4 = bytearray(snap(1_003_000, 4)); p4[184] = 2; p4[185] = 1
                rec(f, 63, 4, 1_003_000, bytes(p4))
                p5 = bytearray(snap(1_004_000, 5)); p5[184] = 9
                rec(f, 63, 5, 1_004_000, bytes(p5))
                # A future telemetry sample must never join an earlier control row.
                rec(f, 64, 2, 9_000_000, INA.pack(9_000_000, 0, 12.0, 0.1, 1.0, 12.0, 1, 0, 0))
                rec(f, 65, 2, 9_000_000, VESC.pack(9_000_000, 0, 24.0, 2.0, 1.0, 0.1, 1000.0, 40.0, 42.0, 10, 1, 1, 0, 0))
                p6 = bytearray(snap(2_000_000, 7)); struct.pack_into("<f", p6, 96, math.nan)
                rec(f, 63, 7, 2_000_000, bytes(p6))
                rec(f, 99, 8, 2_001_000, b"x", version=2)
                rec(f, 66, 9, 2_002_000, b"bad")
                ack = bytearray(WAYPOINT_ACK_STRUCT.size); ack[8] = 3
                rec(f, 67, 10, 2_003_000, bytes(ack))
            result = decode_min_shadow(path)
        self.assertGreater(result["sensor_stale_violations"], 0)
        self.assertGreater(result["invalid_state_transitions"], 0)
        self.assertGreater(result["safety_reason_mismatches"], 0)
        self.assertGreater(result["output_range_violations"], 0)
        self.assertGreater(result["safe_output_violations"], 0)
        self.assertGreater(result["slew_violations"], 0)
        self.assertGreater(result["stop_restart_violations"], 0)
        self.assertGreater(result["ina_temporal_join_errors"], 0)
        self.assertGreater(result["vesc_temporal_join_errors"], 0)
        self.assertGreater(result["sequence_gaps"], 0)
        self.assertGreater(result["unknown_types"], 0)
        self.assertGreater(result["version_errors"], 0)
        self.assertGreater(result["payload_length_errors"], 0)
        self.assertGreater(result["nonfinite"], 0)
        self.assertGreater(result["waypoint_crc_errors"], 0)
        self.assertGreater(result["waypoint_status_errors"], 0)

if __name__ == "__main__":
    unittest.main()







