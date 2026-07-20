#!/usr/bin/env python3
"""Convert VESC_BIN_V1 fixed records. Supports Stage 1A through 1C."""
import argparse,csv,struct,zlib
from pathlib import Path
HEADER_BYTES=256; RECORD_BYTES=160; HM=0x564C4F47; RM=0x56524344
TYPE={1:"VESC_FW_INFO",2:"VESC_VALUES",3:"VESC_RAW_HEADER",4:"VESC_RAW_BLOCK",5:"VESC_COMM_STATUS",6:"SYSTEM_STATUS",9:"LOG_END"}
def crc(b): return zlib.crc32(b)&0xffffffff
def rows(path):
    data=path.read_bytes()
    if len(data)<HEADER_BYTES: raise ValueError("short file")
    h=data[:HEADER_BYTES]
    magic,ver,hb,rb,baud,tx,rx,boot=struct.unpack_from(">I H H H I B B Q",h)
    if (magic,ver,hb,rb)!=(HM,1,HEADER_BYTES,RECORD_BYTES): raise ValueError("unsupported VESC_BIN_V1 header")
    if crc(h[:-4])!=struct.unpack_from(">I",h,HEADER_BYTES-4)[0]: raise ValueError("header CRC error")
    if (len(data)-HEADER_BYTES)%RECORD_BYTES: raise ValueError("partial record")
    header=dict(baud=baud,tx=tx,rx=rx,boot_us=boot)
    for off in range(HEADER_BYTES,len(data),RECORD_BYTES):
        b=data[off:off+RECORD_BYTES]
        magic,ver,typ,flags,size,seq,rxus,requs,rtt,pseq=struct.unpack_from(">I H B B H I Q Q I I",b)
        ok=magic==RM and ver==1 and size==RECORD_BYTES and crc(b[:-4])==struct.unpack_from(">I",b,RECORD_BYTES-4)[0]
        yield dict(type=typ,flags=flags,record_sequence=seq,timestamp_us=rxus,request_timestamp_us=requs,round_trip_us=rtt,packet_sequence=pseq,payload=b[38:-4],crc_ok=ok),header
def write(path,fields,items):
    with path.open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(items)
def common(r): return {k:r[k] for k in ("timestamp_us","request_timestamp_us","round_trip_us","packet_sequence","crc_ok")}
def fw_row(r):
    p=r["payload"]; out=common(r)
    if len(p)<4:return out
    out.update(fw_major=p[0],fw_minor=p[1],response_payload_length=p[2],hardware_name="",firmware_name="",uuid_valid=False)
    pos=3; n=p[pos];pos+=1;out["hardware_name"]=p[pos:pos+n].decode("utf-8","replace");pos+=n
    if pos<len(p): n=p[pos];pos+=1;out["firmware_name"]=p[pos:pos+n].decode("utf-8","replace");pos+=n
    if pos<len(p):out["uuid_valid"]=bool(p[pos])
    return out
def values_row(r):
    p=r["payload"];out=common(r);out["parse_valid"]=bool(r["flags"]&1)
    if len(p)<54:return out
    names=["mos_temp_c","motor_temp_c","motor_current_a","input_current_a","input_voltage_v","duty","erpm","amp_hours","amp_hours_charged","watt_hours","watt_hours_charged"]
    out.update(zip(names,struct.unpack_from("<11f",p,0)))
    out["tachometer"],out["tachometer_abs"]=struct.unpack_from(">ii",p,44)
    out["fault_code"]=p[52];out["response_payload_length"]=p[53]
    return out
def system_row(r):
    p=r["payload"];out=common(r)
    names=["uart_bytes_since_boot","fw_requests","fw_responses","values_requests","values_responses","timeouts","crc_errors","framing_errors","parse_errors","log_drops","sd_write_errors","free_heap"]
    if len(p)>=48:out.update(zip(names,struct.unpack_from(">12I",p,0)))
    return out
def main():
    ap=argparse.ArgumentParser();ap.add_argument("bin",type=Path);args=ap.parse_args()
    groups={i:[] for i in TYPE};bad=0;last=0;end=False;header=None
    for r,header in rows(args.bin):
        if not r["crc_ok"] or (last and r["record_sequence"]!=last+1):bad+=1
        last=r["record_sequence"];groups.setdefault(r["type"],[]).append(r);end|=r["type"]==9
    base=args.bin.with_suffix("")
    cf=["timestamp_us","request_timestamp_us","round_trip_us","packet_sequence","crc_ok"]
    write(Path(str(base)+"_vesc_fw.csv"),cf+["fw_major","fw_minor","response_payload_length","hardware_name","firmware_name","uuid_valid"],[fw_row(r) for r in groups[1]])
    vf=cf+["parse_valid","mos_temp_c","motor_temp_c","motor_current_a","input_current_a","input_voltage_v","duty","erpm","amp_hours","amp_hours_charged","watt_hours","watt_hours_charged","tachometer","tachometer_abs","fault_code","response_payload_length"]
    write(Path(str(base)+"_vesc_values.csv"),vf,[values_row(r) for r in groups[2]])
    write(Path(str(base)+"_vesc_raw.csv"),cf+["record_type","flags","payload_hex"],[{**common(r),"record_type":TYPE.get(r["type"],"UNKNOWN"),"flags":r["flags"],"payload_hex":r["payload"].hex()} for t in (3,4) for r in groups.get(t,[])])
    sf=cf+["uart_bytes_since_boot","fw_requests","fw_responses","values_requests","values_responses","timeouts","crc_errors","framing_errors","parse_errors","log_drops","sd_write_errors","free_heap"]
    write(Path(str(base)+"_system_status.csv"),sf,[system_row(r) for r in groups[6]])
    Path(str(base)+"_summary.txt").write_text(f"format=VESC_BIN_V1\nheader_baud={header['baud']}\nrecords={last}\nrecord_errors={bad}\nlog_end={int(end)}\n",encoding="utf-8")
if __name__=="__main__": main()
