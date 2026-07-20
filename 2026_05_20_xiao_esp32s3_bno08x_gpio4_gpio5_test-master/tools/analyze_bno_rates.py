#!/usr/bin/env python3
"""Calculate per-event received rate and intervals from *_accel/gyro/rotation.csv."""
import argparse,csv,os,statistics

def analyse(path):
 t=[]
 with open(path,encoding='utf-8') as f:
  for r in csv.DictReader(f):t.append(int(r['rx_us']))
 if len(t)<2:return {'count':len(t),'valid':False}
 ds=[b-a for a,b in zip(t,t[1:])];hz=(len(t)-1)*1_000_000/(t[-1]-t[0]);nominal={'accel':5000,'gyro':5000,'rotation':20000}[os.path.basename(path).split('_')[-1].split('.')[0]]
 missing=sum(max(0,round(d/nominal)-1) for d in ds)
 return {'count':len(t),'valid':True,'hz':hz,'avg_us':statistics.mean(ds),'min_us':min(ds),'max_us':max(ds),'jitter_us':statistics.pstdev(ds),'estimated_missing':missing,'over_2x_us':[d for d in ds if d>2*nominal]}
def main():
 p=argparse.ArgumentParser();p.add_argument('prefix',help='RUN0001 (without _accel.csv)');a=p.parse_args();lines=['# BNO08X event-rate analysis','']
 for n in ('accel','gyro','rotation'):
  r=analyse(f'{a.prefix}_{n}.csv');lines.append(f'## {n}\n');lines.extend(f'- {k}: {v}' for k,v in r.items() if k!='over_2x_us');lines.append(f'- intervals over 2x nominal: {r.get("over_2x_us",[])}\n')
 out=f'{a.prefix}_rate_analysis.md';open(out,'w',encoding='utf-8').write('\n'.join(lines));print(out)
if __name__=='__main__':main()
