"""MIN SHADOW BIN decoder and temporal/log integrity checks."""
from __future__ import annotations
import csv
import math
import struct
import zlib
from pathlib import Path
from .binlog import records

CONTROL_OUTPUT=62; CONTROL_SNAPSHOT=63; INA_STATUS=64; VESC_TELEMETRY=65; WAYPOINT_SET=66; WAYPOINT_ACK=67
SNAPSHOT=struct.Struct("<Q5I4d7f6fH8f8f12B"); INA=struct.Struct("<QI4fBBH"); VESC=struct.Struct("<QI7fi4B")
WAYPOINT_SET_STRUCT=struct.Struct("<2I4Bf32dI"); WAYPOINT_ACK_STRUCT=struct.Struct("<2I4BI")
INA_STALE_US=500_000; VESC_STALE_US=500_000; GNSS_STALE_US=500_000; IMU_STALE_US=100_000; TOF_STALE_US=250_000; SLEW_PER_SECOND=50.0
BASE_FIELDS=("queue_us","source_us","sequence","timestamp_us","gnss_age_us","imu_age_us","tof_age_us","cycle","waypoint_revision","latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid","tof_valid","height_valid","waypoint_reached","output_valid","state","safety_reason","mode","active_waypoint")
JOIN_FIELDS=("ina_sample_us","ina_payload_age_us","ina_age_us","ina_source_valid","ina_effective_valid","ina_stale","ina_sample_missing","ina_error_code","bus_voltage_v","shunt_voltage_v","current_a","power_w","vesc_sample_us","vesc_payload_age_us","vesc_age_us","vesc_source_valid","vesc_effective_valid","vesc_stale","vesc_sample_missing","vesc_error_code","vesc_fault","input_voltage_v","motor_current_a","input_current_a","duty","erpm","mos_temp_c","motor_temp_c","tachometer","vesc_mechanical_rpm_valid")
CSV_FIELDS=BASE_FIELDS+JOIN_FIELDS
_SNAPSHOT_FLOAT_FIELDS=("latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit")
_OUTPUTS=("left_front_wing","right_front_wing","rear_yaw","propulsion"); _OUTPUT_LIMITS={key:(-1.0,1.0) for key in _OUTPUTS}; _OUTPUT_LIMITS["propulsion"]=(0.0,1.0)
_REASON_NAMES={0:"NONE",1:"STOP",2:"E_STOP",3:"HEARTBEAT_TIMEOUT",4:"GNSS_INVALID",5:"GNSS_STALE",6:"IMU_INVALID",7:"IMU_STALE",8:"TOF_INVALID",9:"TOF_STALE",10:"NONFINITE",11:"VESC_FAULT"}

def _finite(values): return all(not isinstance(value,float) or math.isfinite(value) for value in values)

def _snapshot(payload):
    if len(payload)!=SNAPSHOT.size: raise ValueError(f"type 63 length {len(payload)} != {SNAPSHOT.size}")
    names=("timestamp_us","cycle","waypoint_revision","gnss_age_us","imu_age_us","tof_age_us","latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid","tof_valid","height_valid","waypoint_reached","output_valid","state","safety_reason","mode","active_waypoint","reserved0","reserved1")
    return dict(zip(names,SNAPSHOT.unpack(payload)))

def _sample(kind,rec):
    if kind=="ina":
        t,a,b,s,c,p,v,e,_=INA.unpack(rec["payload"]); return {"timestamp_us":t,"payload_age_us":a,"valid":bool(v),"error_code":e,"fault":0,"values":(b,s,c,p)}
    t,a,vin,mc,ic,duty,erpm,mos,motor,tach,v,rpm,fault,_=VESC.unpack(rec["payload"])
    return {"timestamp_us":t,"payload_age_us":a,"valid":bool(v),"mechanical_rpm_valid":bool(rpm),"fault":fault,"values":(vin,mc,ic,duty,erpm,mos,motor,tach),"input_voltage_v":vin,"motor_current_a":mc,"input_current_a":ic,"duty":duty,"erpm":erpm,"mos_temp_c":mos,"motor_temp_c":motor,"tachometer":tach}

def _join(sample,control_us,threshold):
    if sample is None or sample["timestamp_us"]>control_us: return None
    age=control_us-sample["timestamp_us"]; return dict(sample,age_us=age,stale=age>threshold,effective_valid=sample["valid"] and age<=threshold and not sample["fault"] and _finite(sample["values"]))

def _empty_output_stats():
    return {key:{"min":math.inf,"max":-math.inf,"non_neutral":0,"changes":0,"safe":0,"range_violations":0,"slew_violations":0,"nonfinite":0} for key in _OUTPUTS}

def _inspect(rows):
    out={"sensor_stale_violations":0,"invalid_state_transitions":0,"safety_reason_mismatches":0,"output_range_violations":0,"safe_output_violations":0,"slew_violations":0,"stop_restart_violations":0,"course_wrap_violations":0,"ina_missing_count":0,"vesc_missing_count":0,"ina_stale_count":0,"vesc_stale_count":0,"ina_invalid_count":0,"vesc_invalid_count":0,"ina_effective_valid_violations":0,"vesc_effective_valid_violations":0,"vesc_fault_count":0,"vesc_fault_violations":0,"first_errors":[],"safety_reason_counts":{},"output_stats":_empty_output_stats()}
    previous=None; previous_generations={"ina":-1,"vesc":-1}; stopped=False
    for row in rows:
        reason=row["safety_reason"]; name=_REASON_NAMES.get(reason,f"UNKNOWN_{reason}"); out["safety_reason_counts"][name]=out["safety_reason_counts"].get(name,0)+1
        for sensor,valid,age,limit in (("gnss",row["gnss_valid"],row["gnss_age_us"],GNSS_STALE_US),("imu",row["imu_valid"],row["imu_age_us"],IMU_STALE_US),("tof",row["tof_valid"],row["tof_age_us"],TOF_STALE_US)):
            if valid and age>limit: out["sensor_stale_violations"]+=1; out["first_errors"].append(f"{sensor}_stale_seq_{row['sequence']}") if len(out["first_errors"])<10 else None
        for sensor,joined in (("ina",row.get("ina_join")),("vesc",row.get("vesc_join"))):
            generation=row.get(f"_{sensor}_generation",-1)
            previous_generation=previous.get(f"_{sensor}_generation",-2) if previous is not None else -2
            if joined is None or (previous is not None and generation == previous_generation): out[f"{sensor}_missing_count"]+=1
            if joined is None: continue
            if joined.get("stale"): out[f"{sensor}_stale_count"]+=1; out["sensor_stale_violations"]+=1
            if not joined.get("valid"): out[f"{sensor}_invalid_count"]+=1
            if not joined.get("valid") and joined.get("effective_valid"): out[f"{sensor}_effective_valid_violations"]+=1
            if sensor=="vesc" and joined.get("fault"): out["vesc_fault_count"]+=1; out["vesc_fault_violations"]+=int(joined.get("effective_valid"))
        state=row["state"]; out["invalid_state_transitions"]+=int(state not in (0,1,2,3))
        dt=0.0 if previous is None else max(1,row["timestamp_us"]-previous["timestamp_us"])/1e6
        if previous is not None and (previous["state"],state) not in {(0,0),(0,1),(0,2),(0,3),(1,0),(1,1),(1,2),(1,3),(2,2),(2,0),(2,3),(3,3),(3,0)}: out["invalid_state_transitions"]+=1
        entering_fault = state == 3 and (previous is None or previous["state"] != 3)
        if state==2 and (previous is None or previous["state"] != 2): out["safety_reason_mismatches"]+=int(reason!=2)
        if entering_fault: out["safety_reason_mismatches"]+=int(reason not in {3,4,5,6,7,8,9,10,11})
        out["course_wrap_violations"]+=int(not math.isfinite(row["course_error_rad"]) or abs(row["course_error_rad"])>math.pi+1e-5)
        explicit_start=state==1 and row.get("mode")==2
        if explicit_start: stopped=False
        elif state in (0,2,3): stopped=True
        if stopped and state==1 and previous is not None and previous["state"] in (0,2,3) and not explicit_start: out["stop_restart_violations"]+=1
        for key in _OUTPUTS:
            value=row[key]; stats=out["output_stats"][key]
            if not math.isfinite(value): stats["nonfinite"]+=1
            else:
                stats["min"]=min(stats["min"],value); stats["max"]=max(stats["max"],value); stats["non_neutral"]+=int(abs(value)>1e-5); stats["safe"]+=int(state!=1 and abs(value)<=1e-5)
                lo,hi=_OUTPUT_LIMITS[key]; bad=value<lo-1e-5 or value>hi+1e-5; stats["range_violations"]+=int(bad); out["output_range_violations"]+=int(bad)
                if previous is not None and previous["state"]==1 and state==1 and abs(value-previous[key])>SLEW_PER_SECOND*dt+1e-5: stats["slew_violations"]+=1; out["slew_violations"]+=1
                stats["changes"]+=int(previous is not None and abs(value-previous[key])>1e-5)
        if state in (0,2,3) and any(abs(row[key])>1e-5 for key in _OUTPUTS): out["safe_output_violations"]+=1
        previous=row
    for stats in out["output_stats"].values():
        if stats["min"]==math.inf: stats["min"]=None
        if stats["max"]==-math.inf: stats["max"]=None
    return out

def decode_min_shadow(bin_path,csv_path=None,txt_path=None):
    recs=list(records(Path(bin_path).read_bytes())); result={"records":len(recs),"version_errors":0,"unknown_types":0,"sequence_gaps":0,"timestamp_reversals":0,"nonfinite":0,"output_range_errors":0,"csv_rows":0,"transport_diagnostics":{},"ina_temporal_join_errors":0,"vesc_temporal_join_errors":0,"payload_length_errors":0,"waypoint_crc_errors":0,"waypoint_status_errors":0}
    expected={CONTROL_SNAPSHOT:SNAPSHOT.size,INA_STATUS:INA.size,VESC_TELEMETRY:VESC.size,WAYPOINT_SET:WAYPOINT_SET_STRUCT.size,WAYPOINT_ACK:WAYPOINT_ACK_STRUCT.size}; known={CONTROL_OUTPUT,CONTROL_SNAPSHOT,INA_STATUS,VESC_TELEMETRY,WAYPOINT_SET,WAYPOINT_ACK}; last_seq={}; last_ts={}; ina=vesc=None; ina_generation=0; vesc_generation=0; rows=[]; temporal=[]
    for rec in recs:
        result["version_errors"]+=int(rec["version"]!=1); typ=rec["type"]; result["unknown_types"]+=int(typ not in known)
        if typ in expected: result["payload_length_errors"]+=int(len(rec["payload"])!=expected[typ])
        boot=rec.get("boot_id",0)
        if boot in last_seq and rec["sequence"]>last_seq[boot]+1: result["sequence_gaps"]+=rec["sequence"]-last_seq[boot]-1
        last_seq[boot]=rec["sequence"]
        if typ==WAYPOINT_SET and len(rec["payload"])==WAYPOINT_SET_STRUCT.size:
            f=WAYPOINT_SET_STRUCT.unpack(rec["payload"]); result["waypoint_crc_errors"]+=int((zlib.crc32(rec["payload"][:-4])&0xffffffff)!=f[-1])
        elif typ==WAYPOINT_ACK and len(rec["payload"])==WAYPOINT_ACK_STRUCT.size:
            f=WAYPOINT_ACK_STRUCT.unpack(rec["payload"]); result["waypoint_crc_errors"]+=int((zlib.crc32(rec["payload"][:-4])&0xffffffff)!=f[-1]); result["waypoint_status_errors"]+=int(f[2] not in (0,1,2))
        if typ in last_ts and rec["source_us"]<last_ts[typ]: result["timestamp_reversals"]+=1
        last_ts[typ]=rec["source_us"]
        if typ==INA_STATUS and len(rec["payload"])==INA.size:
            ina=_sample("ina",rec); ina_generation+=1
        elif typ==VESC_TELEMETRY and len(rec["payload"])==VESC.size:
            vesc=_sample("vesc",rec); vesc_generation+=1
        elif typ==CONTROL_SNAPSHOT and len(rec["payload"])==SNAPSHOT.size:
            row={**rec,**_snapshot(rec["payload"])}; result["nonfinite"]+=int(not _finite([row[key] for key in _SNAPSHOT_FLOAT_FIELDS])); ij=_join(ina,row["timestamp_us"],INA_STALE_US); vj=_join(vesc,row["timestamp_us"],VESC_STALE_US)
            if ina is not None and ina["timestamp_us"]>row["timestamp_us"]: result["ina_temporal_join_errors"]+=1; temporal.append(f"future_ina_seq_{rec['sequence']}")
            if vesc is not None and vesc["timestamp_us"]>row["timestamp_us"]: result["vesc_temporal_join_errors"]+=1; temporal.append(f"future_vesc_seq_{rec['sequence']}")
            row["ina_join"],row["vesc_join"]=ij,vj; row["_ina_generation"]=ina_generation; row["_vesc_generation"]=vesc_generation; rows.append(row)
    result.update(_inspect(rows)); result["output_range_errors"]=result.get("output_range_violations",0); result["temporal_first_errors"]=temporal[:10]
    if csv_path is not None:
        with Path(csv_path).open("w",newline="",encoding="utf-8") as file:
            writer=csv.DictWriter(file,fieldnames=CSV_FIELDS); writer.writeheader()
            for row in rows:
                out={key:row.get(key,"") for key in BASE_FIELDS}; i=row.get("ina_join") or {}; v=row.get("vesc_join") or {}; iv=i.get("values",("",)*4)
                out.update({"ina_sample_us":i.get("timestamp_us",""),"ina_payload_age_us":i.get("payload_age_us",""),"ina_age_us":i.get("age_us",""),"ina_source_valid":i.get("valid",""),"ina_effective_valid":i.get("effective_valid",False),"ina_stale":i.get("stale",True),"ina_sample_missing":not bool(i),"ina_error_code":i.get("error_code",""),"bus_voltage_v":iv[0] if i else "","shunt_voltage_v":iv[1] if i else "","current_a":iv[2] if i else "","power_w":iv[3] if i else "","vesc_sample_us":v.get("timestamp_us",""),"vesc_payload_age_us":v.get("payload_age_us",""),"vesc_age_us":v.get("age_us",""),"vesc_source_valid":v.get("valid",""),"vesc_effective_valid":v.get("effective_valid",False),"vesc_stale":v.get("stale",True),"vesc_sample_missing":not bool(v),"vesc_error_code":0,"vesc_fault":v.get("fault",""),"input_voltage_v":v.get("input_voltage_v",""),"motor_current_a":v.get("motor_current_a",""),"input_current_a":v.get("input_current_a",""),"duty":v.get("duty",""),"erpm":v.get("erpm",""),"mos_temp_c":v.get("mos_temp_c",""),"motor_temp_c":v.get("motor_temp_c",""),"tachometer":v.get("tachometer",""),"vesc_mechanical_rpm_valid":v.get("mechanical_rpm_valid",False)}); writer.writerow(out)
        result["csv_rows"]=len(rows)
    if txt_path is not None and Path(txt_path).exists():
        for line in Path(txt_path).read_text(encoding="utf-8",errors="replace").splitlines():
            if "=" in line:
                key,value=line.split("=",1)
                if key.endswith(("crc_errors","cobs_errors","length_errors","queue_drops","sd_write_errors")):
                    try: result["transport_diagnostics"][key]=int(value)
                    except ValueError: pass
    return result

if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser(); parser.add_argument("bin"); parser.add_argument("--csv"); parser.add_argument("--txt"); args=parser.parse_args(); print(decode_min_shadow(args.bin,args.csv,args.txt))

