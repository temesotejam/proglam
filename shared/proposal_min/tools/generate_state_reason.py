import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SPEC=ROOT/'state_reason.json'
CPP=ROOT/'src'/'state_reason_generated.h'
PY=Path(__file__).resolve().parents[2]/'..'/'pc-tools'/'boat_eskf'/'boat_eskf'/'state_reason_generated.py'
CHECK=sys.argv[1:]==['--check']
spec=json.loads(SPEC.read_text(encoding='utf-8'))
states=spec['states']; reasons=spec['reasons']
cpp=[
 '#pragma once','#include <stdint.h>','namespace proposal_min::state_reason_generated {',
 'constexpr uint8_t kStateDisarmed=%d,kStateRunning=%d,kStateEStop=%d,kStateFault=%d;'%(states['DISARMED'],states['RUNNING'],states['E_STOP'],states['FAULT']),
 'constexpr uint8_t kReasonNone=%d,kReasonStop=%d,kReasonEStop=%d,kReasonHeartbeatTimeout=%d,kReasonGnssInvalid=%d,kReasonGnssStale=%d,kReasonImuInvalid=%d,kReasonImuStale=%d,kReasonTofInvalid=%d,kReasonTofStale=%d,kReasonNonfinite=%d,kReasonVescFault=%d;'%tuple(reasons[k] for k in ('NONE','STOP','E_STOP','HEARTBEAT_TIMEOUT','GNSS_INVALID','GNSS_STALE','IMU_INVALID','IMU_STALE','TOF_INVALID','TOF_STALE','NONFINITE','VESC_FAULT')),
 'constexpr const char* kReasonNames[] = {'+','.join('"%s"'%name for name in reasons)+'};',
 'inline const char* reasonName(uint8_t reason) { return reason < sizeof(kReasonNames)/sizeof(kReasonNames[0]) ? kReasonNames[reason] : "UNKNOWN"; }',
 'constexpr uint8_t kAllowedTransitions[][2] = {'+','.join('{%d,%d}'%tuple(item) for item in spec['allowed_transitions'])+'};',
 'constexpr uint8_t kFaultReasons[] = {'+','.join(map(str,spec['fault_reasons']))+'};','}' ]
cpp_text='\n'.join(cpp)+'\n'
py=['# generated from shared/proposal_min/state_reason.json; do not edit','STATES='+repr(states),'REASONS='+repr(reasons),'REASON_NAMES='+repr({value:name for name,value in reasons.items()}),'ALLOWED_TRANSITIONS='+repr([tuple(item) for item in spec['allowed_transitions']]),'FAULT_REASONS='+repr(tuple(spec['fault_reasons']))]
py_text='\n'.join(py)+'\n'
if CHECK:
    if CPP.read_text(encoding='utf-8')!=cpp_text or PY.read_text(encoding='utf-8')!=py_text: raise SystemExit('generated state/reason files are stale')
else:
    CPP.write_text(cpp_text,encoding='utf-8'); PY.write_text(py_text,encoding='utf-8')
