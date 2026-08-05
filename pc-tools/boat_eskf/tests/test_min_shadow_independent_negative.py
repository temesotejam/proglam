import os
import sys
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from boat_eskf.binlog import MAGIC
from boat_eskf.min_shadow_log import SNAPSHOT, INA, VESC, WAYPOINT_ACK_STRUCT, WAYPOINT_SET_STRUCT, decode_min_shadow
HEADER=struct.Struct("<BBHIIQH")

def snapshot(ts=1_000, seq=1, state=1, reason=0):
    values=[0]*52; values[0]=ts; values[1]=seq; values[6:10]=[35.0,139.0,35.0,139.0]
    values[40:52]=[1,1,1,1,0,1,state,reason,2 if state==1 else 1,0,0,0]
    return SNAPSHOT.pack(*values)

def record(stream, typ, seq, timestamp, payload, version=1):
    stream.write(struct.pack("<IQ",MAGIC,timestamp)); stream.write(HEADER.pack(version,typ,len(payload),seq,7,timestamp,0)); stream.write(payload)

def ina(timestamp, valid=1): return INA.pack(timestamp,0,12.0,0.1,1.0,12.0,valid,0 if valid else 2,0)
def vesc(timestamp, valid=1, fault=0): return VESC.pack(timestamp,0,24.0,2.0,1.0,0.1,1000.0,40.0,42.0,10,valid,1,fault,0)

class IndependentNegativeDiagnosticsTest(unittest.TestCase):
    def decode(self, build):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"negative.BIN"
            with path.open("wb") as stream: build(stream)
            return decode_min_shadow(path)
    def test_ina_future_join_only(self):
        r=self.decode(lambda f:(record(f,64,1,2000,ina(2000)),record(f,65,2,1000,vesc(1000)),record(f,63,3,1000,snapshot())))
        self.assertEqual(r["ina_temporal_join_errors"],1); self.assertEqual(r["vesc_temporal_join_errors"],0); self.assertEqual(r["ina_missing_count"],1); self.assertEqual(r["vesc_missing_count"],0)
    def test_vesc_future_join_only(self):
        r=self.decode(lambda f:(record(f,64,1,1000,ina(1000)),record(f,65,2,2000,vesc(2000)),record(f,63,3,1000,snapshot())))
        self.assertEqual(r["ina_temporal_join_errors"],0); self.assertEqual(r["vesc_temporal_join_errors"],1); self.assertEqual(r["ina_missing_count"],0); self.assertEqual(r["vesc_missing_count"],1)
    def test_ina_missing_only(self):
        r=self.decode(lambda f:(record(f,65,1,1000,vesc(1000)),record(f,63,2,1000,snapshot())))
        self.assertEqual(r["ina_missing_count"],1); self.assertEqual(r["vesc_missing_count"],0); self.assertEqual(r["ina_invalid_count"],0); self.assertEqual(r["ina_stale_count"],0)
    def test_vesc_missing_only(self):
        r=self.decode(lambda f:(record(f,64,1,1000,ina(1000)),record(f,63,2,1000,snapshot())))
        self.assertEqual(r["vesc_missing_count"],1); self.assertEqual(r["ina_missing_count"],0); self.assertEqual(r["vesc_invalid_count"],0); self.assertEqual(r["vesc_stale_count"],0)
    def test_ina_stale_only(self):
        r=self.decode(lambda f:(record(f,64,1,1,ina(1)),record(f,65,2,600001,vesc(600001)),record(f,63,3,600001,snapshot(600001,3))))
        self.assertEqual(r["ina_stale_count"],1); self.assertEqual(r["ina_invalid_count"],0); self.assertEqual(r["ina_temporal_join_errors"],0); self.assertEqual(r["ina_effective_valid_violations"],0)
    def test_vesc_stale_only(self):
        r=self.decode(lambda f:(record(f,64,1,600001,ina(600001)),record(f,65,2,1,vesc(1)),record(f,63,3,600001,snapshot(600001,3))))
        self.assertEqual(r["vesc_stale_count"],1); self.assertEqual(r["vesc_invalid_count"],0); self.assertEqual(r["vesc_temporal_join_errors"],0); self.assertEqual(r["vesc_effective_valid_violations"],0)
    def test_ina_source_invalid_only(self):
        r=self.decode(lambda f:(record(f,64,1,1000,ina(1000,0)),record(f,65,2,1000,vesc(1000)),record(f,63,3,1000,snapshot())))
        self.assertEqual(r["ina_invalid_count"],1); self.assertEqual(r["ina_stale_count"],0); self.assertEqual(r["ina_effective_valid_violations"],0)
    def test_vesc_source_invalid_only(self):
        r=self.decode(lambda f:(record(f,64,1,1000,ina(1000)),record(f,65,2,1000,vesc(1000,0)),record(f,63,3,1000,snapshot())))
        self.assertEqual(r["vesc_invalid_count"],1); self.assertEqual(r["vesc_stale_count"],0); self.assertEqual(r["vesc_effective_valid_violations"],0)
    def test_vesc_fault_only(self):
        r=self.decode(lambda f:(record(f,64,1,1000,ina(1000)),record(f,65,2,1000,vesc(1000,1,7)),record(f,63,3,1000,snapshot())))
        self.assertEqual(r["vesc_fault_count"],1); self.assertEqual(r["vesc_fault_violations"],0); self.assertEqual(r["vesc_invalid_count"],0)
    def test_sequence_gap_only(self):
        r=self.decode(lambda f:(record(f,63,1,1000,snapshot()),record(f,63,3,2000,snapshot(2000,3))))
        self.assertEqual(r["sequence_gaps"],1); self.assertEqual(r["timestamp_reversals"],0); self.assertEqual(r["version_errors"],0)
    def test_timestamp_reversal_only(self):
        r=self.decode(lambda f:(record(f,63,1,2000,snapshot(2000)),record(f,63,2,1000,snapshot(1000,2))))
        self.assertEqual(r["timestamp_reversals"],1); self.assertEqual(r["sequence_gaps"],0)
    def test_version_mismatch_only(self):
        r=self.decode(lambda f:record(f,63,1,1000,snapshot(),version=2))
        self.assertEqual(r["version_errors"],1); self.assertEqual(r["unknown_types"],0); self.assertEqual(r["payload_length_errors"],0)
    def test_unknown_type_only(self):
        r=self.decode(lambda f:record(f,99,1,1000,b"x"))
        self.assertEqual(r["unknown_types"],1); self.assertEqual(r["version_errors"],0)
    def test_payload_length_only(self):
        r=self.decode(lambda f:record(f,63,1,1000,b"x"))
        self.assertEqual(r["payload_length_errors"],1); self.assertEqual(r["unknown_types"],0); self.assertEqual(r["nonfinite"],0)
    def test_nonfinite_only(self):
        p=bytearray(snapshot()); struct.pack_into("<f",p,60,float("nan")); r=self.decode(lambda f:record(f,63,1,1000,bytes(p)))
        self.assertEqual(r["nonfinite"],1); self.assertEqual(r["output_range_violations"],0)
    def test_invalid_state_only(self):
        r=self.decode(lambda f:record(f,63,1,1000,snapshot(state=9)))
        self.assertEqual(r["invalid_state_transitions"],1); self.assertEqual(r["safety_reason_mismatches"],0)
    def test_safety_reason_mismatch_only(self):
        r=self.decode(lambda f:record(f,63,1,1000,snapshot(state=2,reason=0)))
        self.assertEqual(r["safety_reason_mismatches"],1); self.assertEqual(r["invalid_state_transitions"],0)
    def test_output_range_only(self):
        p=bytearray(snapshot()); struct.pack_into("<f",p,158,2.0); r=self.decode(lambda f:record(f,63,1,1000,bytes(p)))
        self.assertEqual(r["output_range_violations"],1); self.assertEqual(r["safe_output_violations"],0)
    def test_safe_output_only(self):
        p=bytearray(snapshot(state=0)); struct.pack_into("<f",p,158,0.2); r=self.decode(lambda f:record(f,63,1,1000,bytes(p)))
        self.assertEqual(r["safe_output_violations"],1); self.assertEqual(r["output_range_violations"],0)
    def test_slew_only(self):
        p1=snapshot(1000,1); p2=bytearray(snapshot(2000,2)); struct.pack_into("<f",p2,146,1.0)
        r=self.decode(lambda f:(record(f,63,1,1000,p1),record(f,63,2,2000,bytes(p2))))
        self.assertGreaterEqual(r["slew_violations"],1); self.assertEqual(r["output_range_violations"],0)
    def test_stop_restart_only(self):
        p=bytearray(snapshot(2000,2,state=1)); p[186]=1; r=self.decode(lambda f:(record(f,63,1,1000,snapshot(1000,1,state=0)),record(f,63,2,2000,bytes(p))))
        self.assertEqual(r["stop_restart_violations"],1); self.assertEqual(r["invalid_state_transitions"],0)
    def test_waypoint_set_crc_only(self):
        p=bytearray(WAYPOINT_SET_STRUCT.size); struct.pack_into("<2I4Bf",p,0,1,1,1,1,0,0,0.5); record_bytes=bytes(p)
        r=self.decode(lambda f:record(f,66,1,1000,record_bytes)); self.assertEqual(r["waypoint_crc_errors"],1); self.assertEqual(r["waypoint_status_errors"],0)
    def test_waypoint_ack_crc_only(self):
        p=bytearray(struct.pack("<2I4BI",1,1,0,0,0,0,0)); struct.pack_into("<I",p,12,1)
        r=self.decode(lambda f:record(f,67,1,1000,bytes(p))); self.assertEqual(r["waypoint_crc_errors"],1); self.assertEqual(r["waypoint_status_errors"],0)
    def test_waypoint_ack_status_only(self):
        p=bytearray(struct.pack("<2I4BI",1,1,3,0,0,0,0)); struct.pack_into("<I",p,12,zlib.crc32(p[:12])&0xffffffff)
        r=self.decode(lambda f:record(f,67,1,1000,bytes(p))); self.assertEqual(r["waypoint_crc_errors"],0); self.assertEqual(r["waypoint_status_errors"],1)

if __name__=="__main__": unittest.main()