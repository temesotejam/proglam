import csv
import os
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from boat_eskf.binlog import MAGIC
from boat_eskf.min_shadow_log import SNAPSHOT, INA, VESC, decode_min_shadow
from boat_eskf.waypoint_protocol import WaypointStore, replace, ACCEPTED, REJECTED, DUPLICATE

HEADER=struct.Struct("<BBHIIQH")

def snap(ts, cycle):
    floats=[0.0]*7+[0.0]*6+[0.0]*8+[0.0]*8
    return SNAPSHOT.pack(ts,cycle,0,0,0,0,35.0,139.0,0.0,0.0,*floats[:7],*floats[7:13],0,*floats[13:21],*floats[21:29],0,0,0,0,0,1,0,0,0,0,0,0)

def record(out, typ, seq, ts, payload):
    out.write(struct.pack("<IQ",MAGIC,ts))
    out.write(HEADER.pack(1,typ,len(payload),seq,7,ts,0))
    out.write(payload)

class MinShadowOperationsTest(unittest.TestCase):
    def test_wire_sizes_are_stable(self):
        self.assertEqual(SNAPSHOT.size,190)
        self.assertEqual(INA.size,32)
        self.assertEqual(VESC.size,48)

    def test_waypoint_all_safety_states_and_atomicity(self):
        for state in ("BOOT","ARMED_IDLE","RUNNING","E_STOP","FAULT"):
            s=WaypointStore(revision=4,points=[(35.0,139.0)])
            status, _=replace(s,[(36.0,140.0)],5,state)
            self.assertEqual(status,REJECTED)
            self.assertEqual((s.revision,s.points),(4,[(35.0,139.0)]))
        s=WaypointStore()
        self.assertEqual(replace(s,[(35.0,139.0)],1,"DISARMED")[0],ACCEPTED)
        self.assertEqual(replace(s,[(0.0,0.0)],1,"DISARMED")[0],DUPLICATE)
        self.assertEqual(replace(s,[(float("nan"),0.0)],2,"DISARMED")[0],REJECTED)
        self.assertEqual(s.points,[(35.0,139.0)])

    def test_temporal_join_never_uses_future_samples(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"run.BIN"
            with path.open("wb") as f:
                record(f,63,1,1000,snap(1000,1))
                record(f,64,1,1500,INA.pack(1500,0,12.0,0.1,1.0,12.0,1,0,0))
                record(f,63,2,2000,snap(2000,2))
                record(f,65,1,2100,VESC.pack(2100,0,24.0,2.0,1.0,0.1,1000.0,40.0,42.0,10,1,0,0,0))
                record(f,63,3,3000,snap(3000,3))
                record(f,64,2,2500,INA.pack(2500,0,13.0,0.2,2.0,26.0,1,0,0))
                record(f,63,4,4000,snap(4000,4))
                record(f,65,2,3500,VESC.pack(3500,0,25.0,3.0,1.5,0.2,1100.0,41.0,43.0,11,1,0,0,0))
                record(f,63,5,5000,snap(5000,5))
            csv_path=Path(d)/"run.csv"
            result=decode_min_shadow(path,csv_path)
            self.assertEqual(result["ina_temporal_join_errors"],0)
            self.assertEqual(result["vesc_temporal_join_errors"],0)
            with csv_path.open(encoding="utf-8") as csv_file:
                rows=list(csv.DictReader(csv_file))
            self.assertEqual(rows[0]["ina_sample_us"],"")
            self.assertEqual(rows[1]["ina_sample_us"],"1500")
            self.assertEqual(rows[2]["ina_sample_us"],"1500")
            self.assertEqual(rows[2]["vesc_sample_us"],"2100")
            self.assertEqual(rows[3]["ina_sample_us"],"2500")
            self.assertEqual(rows[3]["vesc_sample_us"],"2100")
            self.assertEqual(rows[4]["ina_sample_us"],"2500")
            self.assertEqual(rows[4]["vesc_sample_us"],"3500")

if __name__=="__main__":
    unittest.main()
