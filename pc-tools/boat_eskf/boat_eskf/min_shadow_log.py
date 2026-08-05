"""MIN SHADOW BIN decoder, expectation-manifest, and audit evidence writer."""
from __future__ import annotations
import csv, json, math, struct, zlib
from pathlib import Path
from .binlog import records
from .state_reason_generated import REASONS, REASON_NAMES, ALLOWED_TRANSITIONS

CONTROL_OUTPUT=62; CONTROL_SNAPSHOT=63; INA_STATUS=64; VESC_TELEMETRY=65; WAYPOINT_SET=66; WAYPOINT_ACK=67
SNAPSHOT=struct.Struct("<Q5I4d7f6fH8f8f12B"); INA=struct.Struct("<QI4fBBH"); VESC=struct.Struct("<QI7fi4B")
WAYPOINT_SET_STRUCT=struct.Struct("<2I4Bf32dI"); WAYPOINT_ACK_STRUCT=struct.Struct("<2I4BI")
INA_STALE_US=500_000; VESC_STALE_US=500_000; GNSS_STALE_US=500_000; IMU_STALE_US=100_000; TOF_STALE_US=250_000; SLEW_PER_SECOND=50.0
BASE_FIELDS=("queue_us","source_us","sequence","timestamp_us","gnss_age_us","imu_age_us","tof_age_us","cycle","waypoint_revision","latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid","tof_valid","height_valid","waypoint_reached","output_valid","state","safety_reason","mode","active_waypoint")
JOIN_FIELDS=("ina_sample_us","ina_payload_age_us","ina_age_us","ina_source_valid","ina_effective_valid","ina_stale","ina_sample_missing","ina_error_code","bus_voltage_v","shunt_voltage_v","current_a","power_w","vesc_sample_us","vesc_payload_age_us","vesc_age_us","vesc_source_valid","vesc_effective_valid","vesc_stale","vesc_sample_missing","vesc_error_code","vesc_fault","input_voltage_v","motor_current_a","input_current_a","duty","erpm","mos_temp_c","motor_temp_c","tachometer","vesc_mechanical_rpm_valid")
CSV_FIELDS=BASE_FIELDS+JOIN_FIELDS
OUTPUT_FLOAT_FIELDS=("left_front_wing","right_front_wing","rear_yaw","propulsion")
FLOAT_FIELDS=("latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit")
OUTPUTS=("left_front_wing","right_front_wing","rear_yaw","propulsion"); LIMITS={name:(-1.0,1.0) for name in OUTPUTS}; LIMITS["propulsion"]=(0.0,1.0)

def _finite(values): return all(not isinstance(v,float) or math.isfinite(v) for v in values)
def _snapshot(payload):
    names=("timestamp_us","cycle","waypoint_revision","gnss_age_us","imu_age_us","tof_age_us","latitude_deg","longitude_deg","target_waypoint_latitude_deg","target_waypoint_longitude_deg","speed_mps","gnss_course_rad","local_north_m","local_east_m","target_bearing_rad","course_error_rad","waypoint_distance_m","roll_rad","pitch_rad","yaw_rad","roll_rate_rad_s","pitch_rate_rad_s","yaw_rate_rad_s","tof_raw_mm","tof_filtered_m","height_error_m","u_height","u_pitch","u_roll","u_yaw","front_common","front_differential","left_front_wing","right_front_wing","rear_yaw","propulsion","left_prelimit","right_prelimit","rear_yaw_prelimit","propulsion_prelimit","gnss_valid","imu_valid","tof_valid","height_valid","waypoint_reached","output_valid","state","safety_reason","mode","active_waypoint","reserved0","reserved1")
    return dict(zip(names,SNAPSHOT.unpack(payload)))
def _sample(kind, rec):
    if kind=="ina":
        t,a,b,s,c,p,v,e,_=INA.unpack(rec["payload"]); return {"timestamp_us":t,"payload_age_us":a,"valid":bool(v),"error_code":e,"fault":0,"values":(b,s,c,p)}
    t,a,vin,mc,ic,duty,erpm,mos,motor,tach,v,rpm,fault,_=VESC.unpack(rec["payload"])
    return {"timestamp_us":t,"payload_age_us":a,"valid":bool(v),"mechanical_rpm_valid":bool(rpm),"fault":fault,"values":(vin,mc,ic,duty,erpm,mos,motor,tach),"input_voltage_v":vin,"motor_current_a":mc,"input_current_a":ic,"duty":duty,"erpm":erpm,"mos_temp_c":mos,"motor_temp_c":motor,"tachometer":tach}
def _join(sample, control_us, threshold):
    if sample is None or sample["timestamp_us"]>control_us: return None
    age=control_us-sample["timestamp_us"]
    return dict(sample, age_us=age, stale=age>threshold, effective_valid=sample["valid"] and age<=threshold and not sample["fault"] and _finite(sample["values"]))
def _stats(): return {key:{"min":math.inf,"max":-math.inf,"non_neutral":0,"changes":0,"safe":0,"range_violations":0,"slew_violations":0,"nonfinite":0} for key in OUTPUTS}
def _manifest(path, rows):
    if path is None: return None
    raw=json.loads(Path(path).read_text(encoding="utf-8")); cycles=int(raw["cycles"]); default=raw["default"]
    expected={cycle:{"state":int(default["state"]),"reason":int(default["reason"]),"cause":default["cause"]} for cycle in range(cycles)}
    for override in raw.get("overrides",[]): expected[int(override["cycle"])]={"state":int(override["state"]),"reason":int(override["reason"]),"cause":override["cause"]}
    actual_cycles={int(row["cycle"]) for row in rows}
    if actual_cycles != set(range(cycles)): raise ValueError("manifest cycle set does not match BIN snapshots")
    return expected
def _inspect(rows, expected):
    out={"invalid_state_transitions":0,"output_range_violations":0,"safe_output_violations":0,"slew_violations":0,"stop_restart_violations":0,"course_wrap_violations":0,"ina_missing_count":0,"vesc_missing_count":0,"ina_stale_count":0,"vesc_stale_count":0,"ina_invalid_count":0,"vesc_invalid_count":0,"ina_effective_valid_violations":0,"vesc_effective_valid_violations":0,"vesc_fault_count":0,"vesc_fault_violations":0,"first_errors":[],"output_stats":_stats()}
    actual_counts={}; previous=None; stopped=False; mismatches={}; first=None
    for row in rows:
        reason=int(row["safety_reason"]); actual_counts[REASON_NAMES.get(reason,f"UNKNOWN_{reason}")]=actual_counts.get(REASON_NAMES.get(reason,f"UNKNOWN_{reason}"),0)+1
        for sensor,joined in (("ina",row.get("ina_join")),("vesc",row.get("vesc_join"))):
            generation=row.get(f"_{sensor}_generation",-1); previous_generation=previous.get(f"_{sensor}_generation",-2) if previous else -2
            if joined is None or (previous is not None and generation==previous_generation): out[f"{sensor}_missing_count"]+=1; continue
            if joined["stale"]: out[f"{sensor}_stale_count"]+=1
            if not joined["valid"]: out[f"{sensor}_invalid_count"]+=1
            if not joined["valid"] and joined["effective_valid"]: out[f"{sensor}_effective_valid_violations"]+=1
            if sensor=="vesc" and joined["fault"]: out["vesc_fault_count"]+=1; out["vesc_fault_violations"]+=int(joined["effective_valid"])
        state=int(row["state"]); out["invalid_state_transitions"]+=int(state not in (0,1,2,3))
        if previous is not None and (int(previous["state"]),state) not in ALLOWED_TRANSITIONS and not (state==1 and int(row.get("mode",0))==2): out["invalid_state_transitions"]+=1
        if expected is not None:
            exp=expected[int(row["cycle"])]
            if state!=exp["state"] or reason!=exp["reason"]:
                name=REASON_NAMES.get(exp["reason"],f"UNKNOWN_{exp['reason']}"); mismatches[name]=mismatches.get(name,0)+1
                if first is None: first={"cycle":int(row["cycle"]),"timestamp_us":int(row["timestamp_us"]),"cause":exp["cause"],"expected_state":exp["state"],"actual_state":state,"expected_reason":exp["reason"],"actual_reason":reason}
        dt=0.0 if previous is None else max(1,int(row["timestamp_us"])-int(previous["timestamp_us"]))/1e6
        explicit_start=state==1 and int(row.get("mode",0))==2
        if explicit_start: stopped=False
        elif state in (0,2,3): stopped=True
        if stopped and state==1 and previous is not None and int(previous["state"]) in (0,2,3) and not explicit_start: out["stop_restart_violations"]+=1
        for key in OUTPUTS:
            value=row[key]; stat=out["output_stats"][key]
            if not math.isfinite(value): stat["nonfinite"]+=1
            else:
                stat["min"]=min(stat["min"],value); stat["max"]=max(stat["max"],value); stat["non_neutral"]+=int(abs(value)>1e-5); stat["safe"]+=int(state!=1 and abs(value)<=1e-5)
                lo,hi=LIMITS[key]; bad=value<lo-1e-5 or value>hi+1e-5; stat["range_violations"]+=int(bad); out["output_range_violations"]+=int(bad)
                if previous is not None and int(previous["state"])==1 and state==1 and abs(value-previous[key])>SLEW_PER_SECOND*dt+1e-5: stat["slew_violations"]+=1; out["slew_violations"]+=1
                stat["changes"]+=int(previous is not None and abs(value-previous[key])>1e-5)
        if state in (0,2,3) and any(abs(row[key])>1e-5 for key in OUTPUTS): out["safe_output_violations"]+=1
        out["course_wrap_violations"]+=int(not math.isfinite(row["course_error_rad"]) or abs(row["course_error_rad"])>math.pi+1e-5); previous=row
    for stat in out["output_stats"].values():
        if stat["min"]==math.inf: stat["min"]=None
        if stat["max"]==-math.inf: stat["max"]=None
    out["actual_safety_reason_counts"]=actual_counts
    if expected is None:
        out.update(safety_reason_expectation_status="not_provided",expected_safety_reason_counts=None,safety_reason_mismatches=None,safety_reason_mismatches_by_expected_reason=None,first_safety_reason_mismatch=None,safety_reason_expectation_unavailable=None)
    else:
        counts={}
        for item in expected.values():
            name=REASON_NAMES.get(item["reason"],f"UNKNOWN_{item['reason']}"); counts[name]=counts.get(name,0)+1
        out.update(safety_reason_expectation_status="verified",expected_safety_reason_counts=counts,safety_reason_mismatches=sum(mismatches.values()),safety_reason_mismatches_by_expected_reason=mismatches,first_safety_reason_mismatch=first,safety_reason_expectation_unavailable=0)
    return out
def _freeze(records_list, kind, threshold):
    parsed=[(int(rec["source_us"]),_sample(kind,rec)) for rec in records_list if rec["type"]==(INA_STATUS if kind=="ina" else VESC_TELEMETRY) and len(rec["payload"])==(INA.size if kind=="ina" else VESC.size)]
    groups=[]; current=[]
    for source,sample in parsed:
        if source>sample["timestamp_us"]: current.append((source,sample))
        elif current: groups.append(current); current=[]
    if current: groups.append(current)
    group=max(groups,key=len) if groups else []
    if not group: return {"frozen_sample_count":0,"fixed_timestamp":None,"timestamp_change_violations":0,"age_monotonic_violations":0,"stale_expected":0,"stale_actual":0,"first_stale_age_us":None,"recovery_timestamp_us":None,"recovery_failures":1}
    fixed=group[0][1]["timestamp_us"]; ages=[source-sample["timestamp_us"] for source,sample in group]; stale=[age for age in ages if age>threshold]
    end_index=next(i for i,pair in enumerate(parsed) if pair==group[-1]); recovery=parsed[end_index+1] if end_index+1<len(parsed) else None
    recovery_ok=recovery is not None and recovery[1]["timestamp_us"]==recovery[0] and recovery[0]-recovery[1]["timestamp_us"]==0
    return {"frozen_sample_count":len(group),"fixed_timestamp":fixed,"timestamp_change_violations":sum(sample["timestamp_us"]!=fixed for _,sample in group),"age_monotonic_violations":sum(ages[i]-ages[i-1]!=20000 for i in range(1,len(ages))),"stale_expected":sum(age>threshold for age in ages),"stale_actual":len(stale),"first_stale_age_us":stale[0] if stale else None,"recovery_timestamp_us":recovery[1]["timestamp_us"] if recovery else None,"recovery_failures":int(not recovery_ok)}
def _transport(path):
    if path is None: return {"status":"not_provided"}
    data=json.loads(Path(path).read_text(encoding="utf-8")); return {"status":"provided",**data}
def decode_min_shadow(bin_path, csv_path=None, expectation_manifest=None, transport_diagnostics_path=None):
    recs=list(records(Path(bin_path).read_bytes())); expected_lengths={CONTROL_SNAPSHOT:SNAPSHOT.size,INA_STATUS:INA.size,VESC_TELEMETRY:VESC.size,WAYPOINT_SET:WAYPOINT_SET_STRUCT.size,WAYPOINT_ACK:WAYPOINT_ACK_STRUCT.size}; known={CONTROL_OUTPUT,*expected_lengths}; result={"records":len(recs),"version_errors":0,"unknown_types":0,"sequence_gaps":0,"timestamp_reversals":0,"nonfinite":0,"input_nonfinite":0,"payload_length_errors":0,"waypoint_crc_errors":0,"waypoint_status_errors":0,"ina_temporal_join_errors":0,"vesc_temporal_join_errors":0}
    last_seq={}; last_ts={}; ina=vesc=None; ina_generation=vesc_generation=0; rows=[]; same_ina={}; same_vesc={}; temporal=[]
    for rec in recs:
        typ=rec["type"]
        if typ==INA_STATUS and len(rec["payload"])==INA.size: same_ina[_sample("ina",rec)["timestamp_us"]]=_sample("ina",rec)
        elif typ==VESC_TELEMETRY and len(rec["payload"])==VESC.size: same_vesc[_sample("vesc",rec)["timestamp_us"]]=_sample("vesc",rec)
    for rec in recs:
        typ=rec["type"]; result["version_errors"]+=int(rec["version"]!=1); result["unknown_types"]+=int(typ not in known)
        if typ in expected_lengths: result["payload_length_errors"]+=int(len(rec["payload"])!=expected_lengths[typ])
        boot=rec.get("boot_id",0); result["sequence_gaps"]+=max(0,rec["sequence"]-last_seq[boot]-1) if boot in last_seq else 0; last_seq[boot]=rec["sequence"]
        result["timestamp_reversals"]+=int(typ in last_ts and rec["source_us"]<last_ts[typ]); last_ts[typ]=rec["source_us"]
        if typ==WAYPOINT_SET and len(rec["payload"])==WAYPOINT_SET_STRUCT.size: result["waypoint_crc_errors"]+=int(zlib.crc32(rec["payload"][:-4])&0xffffffff!=WAYPOINT_SET_STRUCT.unpack(rec["payload"])[-1])
        elif typ==WAYPOINT_ACK and len(rec["payload"])==WAYPOINT_ACK_STRUCT.size:
            fields=WAYPOINT_ACK_STRUCT.unpack(rec["payload"]); result["waypoint_crc_errors"]+=int(zlib.crc32(rec["payload"][:-4])&0xffffffff!=fields[-1]); result["waypoint_status_errors"]+=int(fields[2] not in (0,1,2))
        if typ==INA_STATUS and len(rec["payload"])==INA.size: ina=_sample("ina",rec); ina_generation+=1
        elif typ==VESC_TELEMETRY and len(rec["payload"])==VESC.size: vesc=_sample("vesc",rec); vesc_generation+=1
        elif typ==CONTROL_SNAPSHOT and len(rec["payload"])==SNAPSHOT.size:
            row={**rec,**_snapshot(rec["payload"])}; result["input_nonfinite"]+=int(not _finite([row[key] for key in FLOAT_FIELDS])); result["nonfinite"]+=int(not _finite([row[key] for key in OUTPUT_FLOAT_FIELDS])); ij=_join(same_ina.get(row["timestamp_us"],ina),row["timestamp_us"],INA_STALE_US); vj=_join(same_vesc.get(row["timestamp_us"],vesc),row["timestamp_us"],VESC_STALE_US)
            if ina and ina["timestamp_us"]>row["timestamp_us"]: result["ina_temporal_join_errors"]+=1; temporal.append(f"future_ina_seq_{rec['sequence']}")
            if vesc and vesc["timestamp_us"]>row["timestamp_us"]: result["vesc_temporal_join_errors"]+=1; temporal.append(f"future_vesc_seq_{rec['sequence']}")
            row.update(ina_join=ij,vesc_join=vj,_ina_generation=ina_generation,_vesc_generation=vesc_generation); rows.append(row)
    expected=_manifest(expectation_manifest,rows); result.update(_inspect(rows,expected)); result["output_range_errors"]=result["output_range_violations"]; result["temporal_first_errors"]=temporal[:10]; result["freeze_diagnostics"]={"ina":_freeze(recs,"ina",INA_STALE_US),"vesc":_freeze(recs,"vesc",VESC_STALE_US)}; result["transport_diagnostics"]=_transport(transport_diagnostics_path)
    if csv_path:
        with Path(csv_path).open("w",newline="",encoding="utf-8") as file:
            writer=csv.DictWriter(file,fieldnames=CSV_FIELDS); writer.writeheader()
            for row in rows:
                i=row.get("ina_join") or {}; v=row.get("vesc_join") or {}; iv=i.get("values",("",)*4); out={key:row.get(key,"") for key in BASE_FIELDS}
                out.update({"ina_sample_us":i.get("timestamp_us",""),"ina_payload_age_us":i.get("payload_age_us",""),"ina_age_us":i.get("age_us",""),"ina_source_valid":i.get("valid",""),"ina_effective_valid":i.get("effective_valid",False),"ina_stale":i.get("stale",True),"ina_sample_missing":not bool(i),"ina_error_code":i.get("error_code",""),"bus_voltage_v":iv[0] if i else "","shunt_voltage_v":iv[1] if i else "","current_a":iv[2] if i else "","power_w":iv[3] if i else "","vesc_sample_us":v.get("timestamp_us",""),"vesc_payload_age_us":v.get("payload_age_us",""),"vesc_age_us":v.get("age_us",""),"vesc_source_valid":v.get("valid",""),"vesc_effective_valid":v.get("effective_valid",False),"vesc_stale":v.get("stale",True),"vesc_sample_missing":not bool(v),"vesc_error_code":0,"vesc_fault":v.get("fault",""),"input_voltage_v":v.get("input_voltage_v",""),"motor_current_a":v.get("motor_current_a",""),"input_current_a":v.get("input_current_a",""),"duty":v.get("duty",""),"erpm":v.get("erpm",""),"mos_temp_c":v.get("mos_temp_c",""),"motor_temp_c":v.get("motor_temp_c",""),"tachometer":v.get("tachometer",""),"vesc_mechanical_rpm_valid":v.get("mechanical_rpm_valid",False)}); writer.writerow(out)
        result["csv_rows"]=len(rows)
    else: result["csv_rows"]=0
    return result
def write_report(result, path):
    target=Path(path)
    if target.suffix.lower()==".json": target.write_text(json.dumps(result,ensure_ascii=False,sort_keys=True,indent=2)+"\n",encoding="utf-8")
    else: target.write_text("\n".join(f"{key}={json.dumps(value,ensure_ascii=False,sort_keys=True)}" for key,value in sorted(result.items()))+"\n",encoding="utf-8")
if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser(); parser.add_argument("bin"); parser.add_argument("--csv"); parser.add_argument("--expectation-manifest"); parser.add_argument("--transport-diagnostics"); parser.add_argument("--report"); args=parser.parse_args()
    output=decode_min_shadow(args.bin,args.csv,args.expectation_manifest,args.transport_diagnostics)
    if args.report: write_report(output,args.report)
    print(json.dumps(output,ensure_ascii=False,sort_keys=True))

