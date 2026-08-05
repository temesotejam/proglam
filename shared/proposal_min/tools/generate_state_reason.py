import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "state_reason.json"
CPP = ROOT / "src" / "state_reason_generated.h"
PY = Path(__file__).resolve().parents[2] / ".." / "pc-tools" / "boat_eskf" / "boat_eskf" / "state_reason_generated.py"

spec = json.loads(SPEC.read_text(encoding="utf-8"))
states = spec["states"]; reasons = spec["reasons"]
cpp = ["#pragma once", "#include <stdint.h>", "namespace proposal_min::state_reason_generated {", "constexpr uint8_t kStateDisarmed=%d,kStateRunning=%d,kStateEStop=%d,kStateFault=%d;" % (states["DISARMED"],states["RUNNING"],states["E_STOP"],states["FAULT"]), "constexpr uint8_t kReasonNone=%d,kReasonStop=%d,kReasonEStop=%d,kReasonHeartbeatTimeout=%d,kReasonGnssInvalid=%d,kReasonGnssStale=%d,kReasonImuInvalid=%d,kReasonImuStale=%d,kReasonTofInvalid=%d,kReasonTofStale=%d,kReasonNonfinite=%d,kReasonVescFault=%d;" % tuple(reasons[k] for k in ("NONE","STOP","E_STOP","HEARTBEAT_TIMEOUT","GNSS_INVALID","GNSS_STALE","IMU_INVALID","IMU_STALE","TOF_INVALID","TOF_STALE","NONFINITE","VESC_FAULT")), "constexpr uint8_t kAllowedTransitions[][2] = {" + ",".join("{%d,%d}" % tuple(x) for x in spec["allowed_transitions"]) + "};", "constexpr uint8_t kFaultReasons[] = {" + ",".join(map(str,spec["fault_reasons"])) + "};", "}"
]
CPP.write_text("\n".join(cpp)+"\n", encoding="utf-8")
py = ["# generated from shared/proposal_min/state_reason.json; do not edit", "STATES="+repr(states), "REASONS="+repr(reasons), "ALLOWED_TRANSITIONS="+repr([tuple(x) for x in spec["allowed_transitions"]]), "FAULT_REASONS="+repr(tuple(spec["fault_reasons"]))]
PY.write_text("\n".join(py)+"\n", encoding="utf-8")
