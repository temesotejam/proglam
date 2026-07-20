#!/usr/bin/env python3
import argparse,csv,os,struct,zlib
MAGIC=b"PCAL"; RECORD=b"PCRD"; SIZE=160
NAMES={1:"PCA_CONFIG",2:"SERVO_COMMAND",3:"SERVO_OUTPUT",4:"SERVO_STATE_CHANGE",5:"ESTOP_EVENT",6:"WATCHDOG_EVENT",7:"PCA_STATUS",8:"SYSTEM_STATUS",9:"LOG_END"}
def writer(path,fields):
 f=open(path,"w",newline="",encoding="utf-8");w=csv.DictWriter(f,fieldnames=fields);w.writeheader();return f,w
def main():
 ap=argparse.ArgumentParser();ap.add_argument("bin");ap.add_argument("--out",required=True);a=ap.parse_args();os.makedirs(a.out,exist_ok=True);stem=os.path.splitext(os.path.basename(a.bin))[0]
 files=[]
 def make(s,fields):
  x=writer(os.path.join(a.out,stem+s),fields);files.append(x);return x[1]
 cmd=make("_servo_command.csv",["timestamp_us","sequence","accepted","source","channel","requested_us","target_us","state","reason"])
 out=make("_servo_output.csv",["timestamp_us","channel","state","target_us","output_us","on_count","off_count","oe_enabled","slew_limited","write_success","full_off","i2c_duration_us"])
 state=make("_state_change.csv",["timestamp_us","before","after","reason","target_us","output_us"])
 sts=make("_pca_status.csv",["timestamp_us","state","oe_enabled","mode1","mode2","prescale","target_us","output_us","i2c_errors","readback_errors","estops","watchdogs","heartbeat_age_ms","max_i2c_us"])
 counts={}
 d=open(a.bin,"rb").read();magic,ver,h,rsize=struct.unpack_from(">4sHHH",d,0)
 if magic!=MAGIC or rsize!=SIZE:raise SystemExit("not PCA9685_SERVO_BIN_V1")
 for off in range(h,len(d),rsize):
  r=d[off:off+rsize]
  if len(r)!=rsize:break
  m,v,t,fl,sz,seq,rx,req,rt,pseq=struct.unpack_from(">4sHBBHIQQII",r,0)
  if m!=RECORD or v!=ver or sz!=rsize or zlib.crc32(r[:-4])&0xffffffff!=struct.unpack_from(">I",r,rsize-4)[0]:continue
  counts[NAMES.get(t,"UNKNOWN")]=counts.get(NAMES.get(t,"UNKNOWN"),0)+1;p=r[38:156]
  if t==2:
   source,ch,requested,target,st,reason=struct.unpack_from(">BBHHBB",p,0);cmd.writerow(dict(timestamp_us=rx,sequence=pseq,accepted=bool(fl&1),source=source,channel=ch,requested_us=requested,target_us=target,state=st,reason=reason))
  elif t==3:
   ch,st,target,output,on,off,oe,dur=struct.unpack_from(">BBHHHHBI",p,0);out.writerow(dict(timestamp_us=rx,channel=ch,state=st,target_us=target,output_us=output,on_count=on,off_count=off,oe_enabled=oe,slew_limited=bool(fl&1),write_success=bool(fl&2),full_off=bool(fl&4),i2c_duration_us=dur))
  elif t==4:
   before,after,reason,target,output=struct.unpack_from(">BBBHH",p,0);state.writerow(dict(timestamp_us=rx,before=before,after=after,reason=reason,target_us=target,output_us=output))
  elif t==7:
   st,oe,m1,m2,pre,target,output,ie,re,es,wd,age,mx=struct.unpack_from(">BBBBBHHIIIIII",p,0);sts.writerow(dict(timestamp_us=rx,state=st,oe_enabled=oe,mode1="0x%02X"%m1,mode2="0x%02X"%m2,prescale=pre,target_us=target,output_us=output,i2c_errors=ie,readback_errors=re,estops=es,watchdogs=wd,heartbeat_age_ms=age,max_i2c_us=mx))
 for f,w in files:f.close()
 print("records",counts)
if __name__=="__main__":main()