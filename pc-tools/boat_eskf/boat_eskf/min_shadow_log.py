"""MIN SHADOW BIN decoder and temporal/log integrity checks."""
from __future__ import annotations
import csv, math, struct, zlib
from pathlib import Path
from typing import Any
from .binlog import records

CONTROL_OUTPUT=62
CONTROL_SNAPSHOT=63
INA_STATUS=64
VESC_TELEMETRY=65
WAYPOINT_SET=66
WAYPOINT_ACK=67
SNAPSHOT=struct.Struct("<Q5I4d7f6fH8f8f12B")
INA=struct.Struct("<QI4fBBH")
VESC=struct.Struct("<QI7fi4B")
INA_STALE_US=500_000
VESC_STALE_US=500_000
GNSS_STALE_US=500_000
IMU_STALE_US=100_000
TOF_STALE_US=250_000
SLEW_PER_SECOND=50.0
WAYPOINT_SET_STRUCT=struct.Struct("<2I4Bf32dI")
WAYPOINT_ACK_STRUCT=struct.Struct("<2I4BI")

BASE_FIELDS=("queue_us","source_us","sequence","timestamp_us","gnss_age_us","imu_age_us","tof_age_us","cycle","waypoint_revision","latitude_deg","longitude_deg",
"target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m",
"local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad",
"roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm","tof_filtered_m","height_error_m","u_height",
"u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw",
"propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid",
"tof_valid","height_valid","waypoint_reached","output_valid","state","safety_reason","mode","active_waypoint")
JOIN_FIELDS=("ina_sample_us","ina_payload_age_us","ina_age_us","ina_source_valid","ina_effective_valid","ina_stale",
"ina_sample_missing","ina_error_code","bus_voltage_v","shunt_voltage_v","current_a","power_w","vesc_sample_us",
"vesc_payload_age_us","vesc_age_us","vesc_source_valid","vesc_effective_valid","vesc_stale","vesc_sample_missing",
"input_voltage_v","motor_current_a","input_current_a","duty","erpm","mos_temp_c","motor_temp_c","tachometer",
"vesc_mechanical_rpm_valid")
CSV_FIELDS=BASE_FIELDS+JOIN_FIELDS

def _finite(values):
    return all(not isinstance(v,float) or math.isfinite(v) for v in values)

def _snapshot(payload):
    if len(payload)!=SNAPSHOT.size:
        raise ValueError(f"type 63 length {len(payload)} != {SNAPSHOT.size}")
    names=("timestamp_us","cycle","waypoint_revision","gnss_age_us","imu_age_us","tof_age_us",
    "latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps",
    "gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m",
    "roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm",
    "tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential",
    "left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit",
    "rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid","tof_valid","height_valid","waypoint_reached",
    "output_valid","state","safety_reason","mode","active_waypoint","reserved0","reserved1")
    return dict(zip(names,SNAPSHOT.unpack(payload)))

def _sample(kind, rec):
    if kind=="ina":
        t, payload_age, bus, shunt, current, power, valid, error, _=INA.unpack(rec["payload"])
        return {"timestamp_us":t,"payload_age_us":payload_age,"valid":bool(valid),"error_code":error,
                "values":(bus,shunt,current,power)}
    t,payload_age,vin,mc,ic,duty,erpm,mos,motor,tach,valid,rpm_valid,fault,_=VESC.unpack(rec["payload"])
    return {"timestamp_us":t,"payload_age_us":payload_age,"valid":bool(valid),"mechanical_rpm_valid":bool(rpm_valid),
            "fault":fault,"values":(vin,mc,ic,duty,erpm,mos,motor,tach),"input_voltage_v":vin,
            "motor_current_a":mc,"input_current_a":ic,"duty":duty,"erpm":erpm,"mos_temp_c":mos,
            "motor_temp_c":motor,"tachometer":tach}

def _join(sample, control_us, threshold):
    if sample is None or sample["timestamp_us"]>control_us:
        return None
    age=control_us-sample["timestamp_us"]
    return dict(sample, age_us=age, stale=age>threshold,
                effective_valid=sample["valid"] and age<=threshold and _finite(sample["values"]))

def _inspect(rows):
    out={"sensor_stale_violations":0,"invalid_state_transitions":0,"safety_reason_mismatches":0,
         "output_range_violations":0,"safe_output_violations":0,"slew_violations":0,"stop_restart_violations":0,
         "first_errors":[]}
    prev=None
    stopped=False
    for row in rows:
        for name, valid, age, limit in (("gnss",row["gnss_valid"],row["gnss_age_us"],GNSS_STALE_US),
                                         ("imu",row["imu_valid"],row["imu_age_us"],IMU_STALE_US),
                                         ("tof",row["tof_valid"],row["tof_age_us"],TOF_STALE_US)):
            if valid and age>limit:
                out["sensor_stale_violations"]+=1
                if len(out["first_errors"])<10: out["first_errors"].append(f"{name}_stale_seq_{row['sequence']}")
        for name, joined in (("ina", row.get("ina_join")), ("vesc", row.get("vesc_join"))):
            if joined is not None and joined.get("stale"):
                out["sensor_stale_violations"] += 1
                if len(out["first_errors"]) < 10:
                    out["first_errors"].append(f"{name}_stale_seq_{row['sequence']}")
        state=row["state"]
        if state not in (0,1,2,3):
            out["invalid_state_transitions"]+=1
        if prev is not None:
            allowed={(0,0),(0,1),(0,2),(0,3),(1,0),(1,1),(1,2),(1,3),(2,2),(2,0),(2,3),(3,3),(3,0)}
            if (prev["state"],state) not in allowed:
                out["invalid_state_transitions"]+=1
            dt=max(1,row["timestamp_us"]-prev["timestamp_us"])/1e6
            for key in ("left_front_wing","right_front_wing","rear_yaw","propulsion"):
                if abs(row[key]-prev[key])>SLEW_PER_SECOND*dt+1e-5 and not (state in (0,2,3) or prev["state"] in (0,2,3)):
                    out["slew_violations"]+=1
        if state==2 and row["safety_reason"]==0 and (prev is None or prev["state"] != 2):
            out["safety_reason_mismatches"]+=1
        explicit_start = state==1 and row.get("mode") == 2
        if explicit_start:
            stopped=False
        elif state in (0,2,3):
            stopped=True
        if stopped and state==1 and prev is not None and prev["state"] in (0,2,3) and not explicit_start:
            out["stop_restart_violations"]+=1
        for key,lo,hi in (("left_front_wing",-1,1),("right_front_wing",-1,1),("rear_yaw",-1,1),("propulsion",0,1)):
            if not lo-1e-5<=row[key]<=hi+1e-5: out["output_range_violations"]+=1
        if state in (0,2,3) and any(abs(row[k])>1e-5 for k in ("left_front_wing","right_front_wing","rear_yaw","propulsion")):
            out["safe_output_violations"]+=1
        prev=row
    return out

def decode_min_shadow(bin_path, csv_path=None, txt_path=None):
    data=Path(bin_path).read_bytes()
    recs=list(records(data))
    result={"records":len(recs),"version_errors":0,"unknown_types":0,"sequence_gaps":0,
            "timestamp_reversals":0,"nonfinite":0,"output_range_errors":0,"csv_rows":0,
            "transport_diagnostics":{},"ina_temporal_join_errors":0,"vesc_temporal_join_errors":0,"payload_length_errors":0,"waypoint_crc_errors":0,"waypoint_status_errors":0}
    expected_lengths={CONTROL_SNAPSHOT:SNAPSHOT.size,INA_STATUS:INA.size,VESC_TELEMETRY:VESC.size,WAYPOINT_SET:WAYPOINT_SET_STRUCT.size,WAYPOINT_ACK:WAYPOINT_ACK_STRUCT.size}
    known={CONTROL_OUTPUT,CONTROL_SNAPSHOT,INA_STATUS,VESC_TELEMETRY,WAYPOINT_SET,WAYPOINT_ACK}
    last_seq={};last_ts={};ina=None;vesc=None;rows=[]; temporal=[]
    for rec in recs:
        if rec["version"]!=1: result["version_errors"]+=1
        typ=rec["type"]
        if typ not in known: result["unknown_types"]+=1
        if typ in expected_lengths and len(rec["payload"]) != expected_lengths[typ]: result.setdefault("payload_length_errors",0); result["payload_length_errors"]+=1
        boot=rec.get("boot_id",0)
        if boot in last_seq and rec["sequence"]>last_seq[boot]+1: result["sequence_gaps"]+=rec["sequence"]-last_seq[boot]-1
        last_seq[boot]=rec["sequence"]
        if typ==WAYPOINT_SET and len(rec["payload"])==WAYPOINT_SET_STRUCT.size:
            fields=WAYPOINT_SET_STRUCT.unpack(rec["payload"]); result.setdefault("waypoint_crc_errors",0); result.setdefault("waypoint_status_errors",0); result["waypoint_crc_errors"] += int((zlib.crc32(rec["payload"][:-4]) & 0xffffffff) != fields[-1])
        elif typ==WAYPOINT_ACK and len(rec["payload"])==WAYPOINT_ACK_STRUCT.size:
            fields=WAYPOINT_ACK_STRUCT.unpack(rec["payload"]); result.setdefault("waypoint_crc_errors",0); result.setdefault("waypoint_status_errors",0); result["waypoint_crc_errors"] += int((zlib.crc32(rec["payload"][:-4]) & 0xffffffff) != fields[-1]); result["waypoint_status_errors"] += int(fields[2] not in (0,1,2))
        if typ in last_ts and rec["source_us"]<last_ts[typ]: result["timestamp_reversals"]+=1
        last_ts[typ]=rec["source_us"]
        if typ==INA_STATUS and len(rec["payload"])==INA.size: ina=_sample("ina",rec)
        elif typ==VESC_TELEMETRY and len(rec["payload"])==VESC.size: vesc=_sample("vesc",rec)
        elif typ==CONTROL_SNAPSHOT:
            row={**rec,**_snapshot(rec["payload"])}
            vals=[row[k] for k in ("u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit")]
            if row["gnss_valid"]: vals += [row[k] for k in ("latitude_deg","longitude_deg","speed_mps","gnss_course_rad")]
            if row["imu_valid"]: vals += [row[k] for k in ("roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s")]
            if row["tof_valid"]: vals += [row[k] for k in ("tof_filtered_m","height_error_m")]
            if not _finite(vals): result["nonfinite"]+=1
            ij=_join(ina,row["timestamp_us"],INA_STALE_US)
            vj=_join(vesc,row["timestamp_us"],VESC_STALE_US)
            if ina is not None and ina["timestamp_us"]>row["timestamp_us"]: result["ina_temporal_join_errors"]+=1;temporal.append(f"future_ina_seq_{rec['sequence']}")
            if vesc is not None and vesc["timestamp_us"]>row["timestamp_us"]: result["vesc_temporal_join_errors"]+=1;temporal.append(f"future_vesc_seq_{rec['sequence']}")
            row["ina_join"]=ij;row["vesc_join"]=vj;rows.append(row)
    result.update(_inspect(rows))
    result["output_range_errors"]=result.get("output_range_violations",0)
    result["temporal_first_errors"]=temporal[:10]
    if csv_path is not None:
        with Path(csv_path).open("w",newline="",encoding="utf-8") as f:
            w=csv.DictWriter(f,fieldnames=CSV_FIELDS);w.writeheader()
            for row in rows:
                out={k:row.get(k,"") for k in BASE_FIELDS};i=row.get("ina_join") or {};v=row.get("vesc_join") or {}
                iv=i.get("values",("",)*4);vv=v.get("values",("",)*8)
                out.update({"ina_sample_us":i.get("timestamp_us",""),"ina_payload_age_us":i.get("payload_age_us",""),
                "ina_age_us":i.get("age_us",""),"ina_source_valid":i.get("valid",""),"ina_effective_valid":i.get("effective_valid",False),
                "ina_stale":i.get("stale",True),"ina_sample_missing":not bool(i),"ina_error_code":i.get("error_code",""),
                "bus_voltage_v":iv[0] if i else "","shunt_voltage_v":iv[1] if i else "","current_a":iv[2] if i else "","power_w":iv[3] if i else "",
                "vesc_sample_us":v.get("timestamp_us",""),"vesc_payload_age_us":v.get("payload_age_us",""),
                "vesc_age_us":v.get("age_us",""),"vesc_source_valid":v.get("valid",""),"vesc_effective_valid":v.get("effective_valid",False),
                "vesc_stale":v.get("stale",True),"vesc_sample_missing":not bool(v),"input_voltage_v":v.get("input_voltage_v",""),
                "motor_current_a":v.get("motor_current_a",""),"input_current_a":v.get("input_current_a",""),"duty":v.get("duty",""),
                "erpm":v.get("erpm",""),"mos_temp_c":v.get("mos_temp_c",""),"motor_temp_c":v.get("motor_temp_c",""),
                "tachometer":v.get("tachometer",""),"vesc_mechanical_rpm_valid":v.get("mechanical_rpm_valid",False)})
                w.writerow(out)
        result["csv_rows"]=len(rows)
    if txt_path is not None and Path(txt_path).exists():
        for line in Path(txt_path).read_text(encoding="utf-8",errors="replace").splitlines():
            if "=" in line:
                k,v=line.split("=",1)
                if k.endswith(("crc_errors","cobs_errors","length_errors","queue_drops","sd_write_errors")):
                    try: result["transport_diagnostics"][k]=int(v)
                    except ValueError: pass
    return result

if __name__=="__main__":
    import argparse
    p=argparse.ArgumentParser();p.add_argument("bin");p.add_argument("--csv");p.add_argument("--txt");a=p.parse_args()
    print(decode_min_shadow(a.bin,a.csv,a.txt))


