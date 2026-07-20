#!/usr/bin/env python3
"""Convert BNOLOG v1/v2/v3 binary event logs to per-type CSV. Standard library only."""
import argparse,csv,struct,sys,zlib
HEADER_SIZE=256; HMAGIC=0x424E4F48; RMAGIC=0x424E4F52
NAMES={1:'accel',2:'gyro',3:'rotation',4:'system',5:'reinit',6:'i2c_stall',7:'gnss_raw',8:'gnss_fix',9:'gnss_status'}
OLD_FMT='<IHBBHIQIQHH7f10xI'; OLD_SIZE=80
BNO_HEADER=['sequence','rx_us','delta_us','sensor_us','sensor_time_valid','accuracy','sensor_sequence','fault_code','v0','v1','v2','v3','v4','v5','v6']
FIX_HEADER=['sequence','nmea_rx_end_us','delta_us','valid_flags','fix_valid','lat_valid','lon_valid','alt_valid','speed_valid','course_valid','sats_valid','hdop_valid','utc_time','utc_date','fix_type','dr_state','satellites','latitude_deg','longitude_deg','altitude_m','speed_mps','course_deg','hdop','pdop','vdop','data_age_ms','parse_complete_us','sentence_type']
RAW_HEADER=['sequence','rx_end_us','delta_us','fault_code','nmea_type','checksum_valid','truncated','nmea']
STATUS_HEADER=['sequence','rx_us','bytes_per_s','sentences_per_s','valid_sentences_per_s','checksum_errors_per_s','parse_errors_per_s','overlong_per_s','gnss_log_drops_per_s','last_rx_age_ms','last_fix_age_ms','fix_valid','uart_overflow_observable']
def crc_ok(b):return (zlib.crc32(b[:-4])&0xffffffff)==struct.unpack_from('<I',b,len(b)-4)[0]
def text(b):return b.split(b'\0',1)[0].decode('ascii','replace')
def flag(x,n):return 1 if x&(1<<n) else 0
def main():
 ap=argparse.ArgumentParser();ap.add_argument('bin');ap.add_argument('-o','--out',default=None);a=ap.parse_args();base=a.out or a.bin.rsplit('.',1)[0]
 with open(a.bin,'rb') as f:
  h=f.read(HEADER_SIZE)
  if len(h)!=HEADER_SIZE or struct.unpack_from('<I',h)[0]!=HMAGIC or not crc_ok(h):sys.exit('invalid header or CRC')
  ver,hs,rs=struct.unpack_from('<HHH',h,4)
  if ver not in (1,2,3) or hs!=HEADER_SIZE:sys.exit('unsupported log format')
  if ver in (1,2) and rs!=OLD_SIZE:sys.exit('unexpected v1/v2 record size')
  if ver==3 and rs<42:sys.exit('invalid v3 record size')
  files={};writers={}
  try:
   for typ,name in NAMES.items():
    files[name]=open(f'{base}_{name}.csv','w',newline='',encoding='utf-8');writers[name]=csv.writer(files[name])
    writers[name].writerow(RAW_HEADER if typ==7 else FIX_HEADER if typ==8 else STATUS_HEADER if typ==9 else BNO_HEADER)
   index=0
   while True:
    b=f.read(rs)
    if not b:break
    index+=1
    if len(b)!=rs:print(f'warning: short record at {index}',file=sys.stderr);break
    if not crc_ok(b):print(f'warning: corrupt record {index}',file=sys.stderr);continue
    if ver in (1,2):
     x=struct.unpack(OLD_FMT,b);magic,rver,typ,acc,length,seq,rx,delta,sensor,fault,flags,*tail=x;payload=struct.pack('<7f',*tail[:7])
    else:
     magic,rver,typ,acc,length,seq,rx,delta,sensor,fault,flags=struct.unpack_from('<IHBBHIQIQHH',b);payload=b[38:-4]
    if magic!=RMAGIC or rver!=ver or length!=rs:print(f'warning: invalid record {index}',file=sys.stderr);continue
    name=NAMES.get(typ)
    if not name:continue
    if typ<=6:
     vals=struct.unpack_from('<7f',payload+b'\0'*28)[:7]
     writers[name].writerow([seq,rx,delta,sensor,flags&1,acc,(flags>>8)&255,fault,*vals])
    elif typ==7:
     n=min(payload[0],max(0,len(payload)-10));writers[name].writerow([seq,rx,delta,fault,text(payload[4:10]),payload[2],payload[3],payload[10:10+n].decode('ascii','replace')])
    elif typ==8:
     if len(payload)<87:print(f'warning: short GNSS_FIX {index}',file=sys.stderr);continue
     fl=struct.unpack_from('<I',payload,0)[0];lat=struct.unpack_from('<d',payload,28)[0];lon=struct.unpack_from('<d',payload,36)[0];vals=struct.unpack_from('<6f',payload,44);age=struct.unpack_from('<I',payload,68)[0];done=struct.unpack_from('<Q',payload,72)[0]
     writers[name].writerow([seq,rx,delta,fl,*[flag(fl,i) for i in range(8)],text(payload[4:16]),text(payload[16:24]),payload[24],struct.unpack_from('<b',payload,25)[0],payload[26],lat,lon,*vals,age,done,text(payload[80:87])])
    else:
     if len(payload)<34:print(f'warning: short GNSS_STATUS {index}',file=sys.stderr);continue
     v=struct.unpack_from('<9I',payload,0);writers[name].writerow([seq,rx,*v,payload[36],payload[37]])
  finally:
   for q in files.values():q.close()
if __name__=='__main__':main()