from .binlog import records
import argparse
p=argparse.ArgumentParser();p.add_argument('bin');a=p.parse_args()
with open(a.bin,'rb') as f:
 for r in records(f.read()):print(r['type'],r['sequence'],r['length'])