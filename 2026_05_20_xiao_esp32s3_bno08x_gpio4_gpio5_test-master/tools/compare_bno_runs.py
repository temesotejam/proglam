#!/usr/bin/env python3
"""Compare converted BNOLOG runs. Usage: python tools/compare_bno_runs.py analysis/run0001/RUN0001 analysis/run0003/RUN0003"""
import csv,sys,os
sys.path.insert(0,os.path.dirname(__file__))
from analyze_bno_rates import analyse
print('|run|type|records|seq_missing|estimated_generated_hz|host_hz|receive_pct|')
print('|-|-:|-:|-:|-:|-:|-:|')
for prefix in sys.argv[1:]:
 for n in ('accel','gyro','rotation'):
  path=prefix+'_'+n+'.csv';r=analyse(path); rows=list(csv.DictReader(open(path,encoding='utf-8')));seq=[int(x['sensor_sequence']) for x in rows];missing=sum((b-a-1)%256 for a,b in zip(seq,seq[1:]));duration=(int(rows[-1]['rx_us'])-int(rows[0]['rx_us']))/1e6 if len(rows)>1 else 0;gen=(len(rows)+missing)/duration if duration else 0;pct=100*len(rows)/(len(rows)+missing) if rows else 0;print(f'|{os.path.basename(prefix)}|{n}|{len(rows)}|{missing}|{gen:.2f}|{r.get("hz",0):.2f}|{pct:.1f}|')
