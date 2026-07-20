import pathlib, struct, tempfile, unittest
from analyze_benchmark import MAGIC, analyze
class BenchmarkToolTest(unittest.TestCase):
 def test_old_style_record_is_compatible(self):
  raw=struct.pack('<IQBBHIIQH',MAGIC,100,1,32,0,1,2,3,0)
  with tempfile.TemporaryDirectory() as d:
   src=pathlib.Path(d)/'old.BIN';src.write_bytes(raw);out=pathlib.Path(d)/'out';n,_=analyze(src,out)
   self.assertEqual(n,1);self.assertTrue((out/'summary.md').exists())
if __name__=='__main__': unittest.main()
