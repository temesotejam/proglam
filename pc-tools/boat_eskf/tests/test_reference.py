import os,struct,sys,unittest
sys.path.insert(0,os.path.dirname(os.path.dirname(__file__)))
from boat_eskf.quaternion import *
from boat_eskf.coordinates import ned
from boat_eskf.eskf import ShadowEskf
from boat_eskf.timebase import estimate_offset
from boat_eskf.measurements import tof_quality
from boat_eskf.binlog import MAGIC,records
from boat_eskf.synthetic import stationary
class TestReference(unittest.TestCase):
 def test_quaternion_group_and_so3(self):
  q=normalize((2,1,0,0)); self.assertAlmostEqual(multiply(q,inverse(q))[0],1,6); v=(.1,-.2,.3); self.assertTrue(all(abs(a-b)<1e-6 for a,b in zip(v,log_so3(exp_so3(v))))); self.assertEqual(wrap_pi(4*3.141592653589793),0)
 def test_coordinates_ned(self):
  self.assertLess(abs(ned(35,139,10,(35,139,10))[0]),1e-5); self.assertGreater(ned(35.0001,139,10,(35,139,10))[0],10)
 def test_static_60_seconds_and_time_reversal(self):
  e=ShadowEskf()
  for t,g,a in stationary():e.predict(t,g,a)
  self.assertLess(abs(e.v[2]),.1); self.assertEqual(e.time_reversals,0); e.predict(1,(0,0,0),(0,0,-9.8)); self.assertEqual(e.time_reversals,1)
 def test_gnss_duplicate_and_straight(self):
  e=ShadowEskf();e.predict(0,(0,0,0),(0,0,-9.8));e.predict(10000,(0,0,0),(0,0,-9.8));self.assertTrue(e.gnss(10000,1,35,139,0,1,0));self.assertFalse(e.gnss(20000,1,35.1,139.1,0,1,0));self.assertTrue(e.gnss(30000,2,35.00001,139,0,2,0));self.assertGreater(e.v[0],0)
 def test_tof_gates(self):
  e=ShadowEskf();self.assertFalse(e.tof(1,0.1,3));self.assertFalse(e.tof(5,0.1,8));self.assertTrue(e.tof(1,0.01,8))
 def test_time_sync(self):self.assertEqual(estimate_offset(100,130,131,161),(0,60,30))
 def test_zone_quality(self):
  n,m,s=tof_quality([1000]*8,[5]*8);self.assertEqual(n,8);self.assertAlmostEqual(m,1);self.assertEqual(s,0)
 def test_bin_decode_and_truncation_recovery(self):
  h=struct.pack('<BBHIIQH',1,56,3,7,8,9,0);d=b'bad'+struct.pack('<IQ',MAGIC,44)+h+b'abc';self.assertEqual(list(records(d))[0]['type'],56);self.assertEqual(len(list(records(d[:-1]))),0)
 def test_replay_determinism(self):
  a=ShadowEskf();b=ShadowEskf();samples=stationary(1,20)
  for x in samples:a.predict(*x);b.predict(*x)
  self.assertEqual(a.p,b.p)
if __name__=='__main__':unittest.main()