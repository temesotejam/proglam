#!/usr/bin/env python3
"""Decode old BOATLOG and new BENCH GOLB records without changing old logs."""
from __future__ import annotations
import argparse, csv, pathlib, struct
from collections import Counter, defaultdict

MAGIC = 0x424C4F47
HEADER = struct.Struct("<BBHIIQH")
RECORD_HEAD = struct.Struct("<IQ")
TYPES = {5:"tof", 6:"ina", 11:"time_sync", 15:"gnss_nav", 16:"gnss_result",
         32:"heartbeat", 48:"phase_prepare", 49:"phase_ready", 50:"phase_start",
         51:"phase_stop", 52:"phase_result", 53:"benchmark_event", 54:"synthetic"}

def records(path: pathlib.Path):
    data = path.read_bytes(); pos = 0
    while pos < len(data):
        if pos + RECORD_HEAD.size + HEADER.size > len(data):
            raise ValueError(f"truncated header at {pos}")
        magic, received_us = RECORD_HEAD.unpack_from(data, pos)
        if magic != MAGIC: raise ValueError(f"bad magic at {pos}")
        h = HEADER.unpack_from(data, pos + RECORD_HEAD.size)
        version, typ, length, seq, boot, source_us, flags = h
        end = pos + RECORD_HEAD.size + HEADER.size + length
        if end > len(data): raise ValueError(f"truncated payload at {pos}")
        yield dict(offset=pos, received_us=received_us, version=version, type=typ,
                   name=TYPES.get(typ, f"type_{typ}"), length=length, sequence=seq,
                   boot_id=boot, source_us=source_us, flags=flags,
                   payload=data[pos + RECORD_HEAD.size + HEADER.size:end])
        pos = end
    return len(data)

def write_csv(path, rows, fields):
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)

def analyze(source: pathlib.Path, output: pathlib.Path):
    output.mkdir(parents=True, exist_ok=True); rows=list(records(source)); counts=Counter(r["name"] for r in rows)
    duration=(rows[-1]["received_us"]-rows[0]["received_us"])/1e6 if len(rows)>1 else 0
    phases=[]; errors=[]; boots=defaultdict(lambda: [0,None,0])
    for r in rows:
        b=boots[r["boot_id"]]; b[0]+=1
        if b[1] is not None and r["sequence"] != (b[1]+1)&0xffffffff: b[2]+=1
        b[1]=r["sequence"]
        if r["type"]==53 and len(r["payload"])>=28:
            campaign, phase, code, status, value, flags, ts, crc=struct.unpack_from("<IHBBIIQI",r["payload"])
            phases.append(dict(campaign_id=campaign,phase_id=phase,event_code=code,status=status,value=value,flags=flags,timestamp_us=ts))
        if r["type"]==52 and len(r["payload"])>=76:
            campaign,phase,kind,status,flags,*values=struct.unpack_from("<IHBBI16I",r["payload"])
            phases.append(dict(campaign_id=campaign,phase_id=phase,event_code=5,status=status,value=values[3],flags=flags,timestamp_us=r["received_us"]))
        if r["type"] in (48,49,50,51,52,53) and r["name"].startswith("type_"): errors.append(dict(offset=r["offset"],error="unknown benchmark record"))
    phase_fields=["campaign_id","phase_id","event_code","status","value","flags","timestamp_us"]
    write_csv(output/"phase_summary.csv",phases,phase_fields)
    for name in ("preflight","ina_profiles","tof_profiles","i2c_comparison","uart_load","sd_performance","time_sync","task_performance"):
        write_csv(output/f"{name}.csv",[],["note"])
    write_csv(output/"errors.csv",errors,["offset","error"])
    with (output/"summary.md").open("w",encoding="utf-8") as f:
        f.write(f"# BENCH解析結果\n\n入力: `{source}`\n\n- records: {len(rows)}\n- duration_s: {duration:.3f}\n- structure: 完全\n\n## 種別\n\n")
        for name,n in sorted(counts.items()): f.write(f"- {name}: {n}\n")
        f.write("\n## Boot連番\n\n")
        for boot,(n,_,gaps) in sorted(boots.items()): f.write(f"- 0x{boot:08X}: records={n}, sequence_gaps={gaps}\n")
        f.write("\n実機のPASS/WARN/FAILはTXT、各phase_result、SDエラーと実測センサ値を併せて判断してください。\n")
    return len(rows), duration

def main():
    p=argparse.ArgumentParser(); p.add_argument("bin",type=pathlib.Path); p.add_argument("--output",type=pathlib.Path,required=True); a=p.parse_args()
    n,d=analyze(a.bin,a.output); print(f"records={n} duration_s={d:.3f} output={a.output}")
if __name__ == "__main__": main()
