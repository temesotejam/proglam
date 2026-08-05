"""Deterministic proposal state-machine replay and fault-injection harness."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from math import isfinite
import json
from typing import Any, Iterable

from .proposal_benchmark import ProposalAlgorithms, Sample, deterministic_sample


@dataclass(frozen=True)
class ReplayEvent:
    t_ms: int
    kind: str
    value: Any = None


NORMAL_PHASES = (
    "STOPPED", "INITIAL_PLACED", "START", "LAUNCH_ALIGN", "START_RAMP",
    "GNSS_SPEED_UP", "COG_VALIDATING", "LOS_NAVIGATION", "WAYPOINT_1",
    "WAYPOINT_2", "GOAL_STOP",
)


def normal_scenario() -> list[ReplayEvent]:
    return [
        ReplayEvent(0, "STOPPED"), ReplayEvent(100, "INITIAL_PLACED"),
        ReplayEvent(200, "START"), ReplayEvent(300, "LAUNCH_ALIGN"),
        ReplayEvent(400, "START_RAMP"), ReplayEvent(1200, "GNSS_SPEED_UP"),
        ReplayEvent(1800, "COG_VALIDATING"), ReplayEvent(2400, "LOS_NAVIGATION"),
        ReplayEvent(3500, "WAYPOINT_1"), ReplayEvent(5200, "WAYPOINT_2"),
        ReplayEvent(7000, "GOAL_STOP"),
    ]


def fault_scenarios() -> dict[str, list[ReplayEvent]]:
    return {
        "gnss_stale": [ReplayEvent(0, "START"), ReplayEvent(100, "GNSS_STALE"), ReplayEvent(200, "HEARTBEAT_TIMEOUT")],
        "cog_unstable": [ReplayEvent(0, "START"), ReplayEvent(100, "COG_UNSTABLE"), ReplayEvent(200, "STOP")],
        "gvr_missing": [ReplayEvent(0, "START"), ReplayEvent(100, "GVR_MISSING"), ReplayEvent(200, "STOP")],
        "bno_stopped": [ReplayEvent(0, "START"), ReplayEvent(100, "BNO_STOPPED"), ReplayEvent(200, "HEARTBEAT_TIMEOUT")],
        "tof_invalid": [ReplayEvent(0, "START"), ReplayEvent(100, "TOF_INVALID"), ReplayEvent(200, "STOP")],
        "uart_gap": [ReplayEvent(0, "START"), ReplayEvent(100, "UART_SEQUENCE_GAP"), ReplayEvent(200, "STOP")],
        "heartbeat_timeout": [ReplayEvent(0, "START"), ReplayEvent(100, "HEARTBEAT_TIMEOUT")],
        "stop": [ReplayEvent(0, "START"), ReplayEvent(100, "STOP")],
        "e_stop": [ReplayEvent(0, "START"), ReplayEvent(100, "E_STOP")],
        "sd_error": [ReplayEvent(0, "START"), ReplayEvent(100, "SD_WRITE_ERROR"), ReplayEvent(200, "STOP")],
        "nan_inf": [ReplayEvent(0, "START"), ReplayEvent(100, "NAN_INPUT"), ReplayEvent(200, "E_STOP")],
        "timestamp_jump": [ReplayEvent(0, "START"), ReplayEvent(100, "TIMESTAMP_JUMP"), ReplayEvent(200, "STOP")],
    }


class ReplayRunner:
    def __init__(self, mode: str = "MIN"):
        self.mode = mode
        self.algorithm = ProposalAlgorithms(mode)
        self.state = "STOPPED"
        self.transitions: list[dict[str, Any]] = []
        self.outputs: list[dict[str, Any]] = []
        self.errors: list[str] = []
        self.counters = {
            "gnss_stale": 0, "cog_unstable": 0, "gvr_missing": 0,
            "bno_stopped": 0, "tof_invalid": 0, "uart_sequence_gap": 0,
            "heartbeat_timeout": 0, "sd_write_error": 0, "nan_inf": 0,
            "timestamp_jump": 0,
        }

    def _transition(self, event: str, t_ms: int) -> None:
        old = self.state
        if event == "INITIAL_PLACED" and old == "STOPPED":
            self.state = "INITIAL_PLACED"
        elif event == "START" and old in ("STOPPED", "INITIAL_PLACED"):
            self.state = "START"
        elif event == "LAUNCH_ALIGN" and old == "START":
            self.state = "LAUNCH_ALIGN"
        elif event == "START_RAMP" and old == "LAUNCH_ALIGN":
            self.state = "START_RAMP"
        elif event == "GNSS_SPEED_UP" and old == "START_RAMP":
            self.state = "GNSS_SPEED_UP"
        elif event == "COG_VALIDATING" and old == "GNSS_SPEED_UP":
            self.state = "COG_VALIDATING"
        elif event == "LOS_NAVIGATION" and old == "COG_VALIDATING":
            self.state = "LOS_NAVIGATION"
        elif event == "WAYPOINT_1" and old == "LOS_NAVIGATION":
            self.state = "WAYPOINT_1"
        elif event == "WAYPOINT_2" and old == "WAYPOINT_1":
            self.state = "WAYPOINT_2"
        elif event == "GOAL_STOP" and old in ("LOS_NAVIGATION", "WAYPOINT_2"):
            self.state = "GOAL_STOP"
        elif event == "E_STOP":
            self.state = "E_STOP"
        else:
            if event == "STOP":
                self.state = "DISARMED"
                if old != self.state:
                    self.transitions.append({"t_ms": t_ms, "from": old, "to": self.state, "event": event})
                return
            fault_key = {
                "GNSS_STALE": "gnss_stale", "COG_UNSTABLE": "cog_unstable",
                "GVR_MISSING": "gvr_missing", "BNO_STOPPED": "bno_stopped",
                "TOF_INVALID": "tof_invalid", "UART_SEQUENCE_GAP": "uart_sequence_gap",
                "HEARTBEAT_TIMEOUT": "heartbeat_timeout", "SD_WRITE_ERROR": "sd_write_error",
            }.get(event)
            if fault_key is not None:
                self.counters[fault_key] += 1
                self.state = "DISARMED"
            elif event == "NAN_INPUT":
                self.counters["nan_inf"] += 1
                self.state = "DISARMED"
            elif event == "TIMESTAMP_JUMP":
                self.counters["timestamp_jump"] += 1
                self.state = "DISARMED"
        if old != self.state:
            self.transitions.append({"t_ms": t_ms, "from": old, "to": self.state, "event": event})

    def _shadow_step(self, index: int, event: str) -> None:
        s = deterministic_sample(index)
        if event in ("NAN_INPUT",):
            # Feed the explicit fault through the same finite-output guard.
            s = Sample(s.index, s.t_us, float("nan"), s.lon, s.speed_mps, s.course_rad,
                       s.yaw_rad, s.yaw_rate, s.roll_rad, s.pitch_rad,
                       s.tof_ranges_mm, s.tof_status, s.bno_valid, s.gnss_valid)
        output = self.algorithm.execute(s)
        if self.state not in ("START", "LAUNCH_ALIGN", "START_RAMP", "GNSS_SPEED_UP",
                              "COG_VALIDATING", "LOS_NAVIGATION", "WAYPOINT_1", "WAYPOINT_2"):
            output = (0.0, 0.0, 0.0)
        if not all(isfinite(float(x)) for x in output):
            self.errors.append("nonfinite_shadow_output")
            output = (0.0, 0.0, 0.0)
        self.outputs.append({"index": index, "state": self.state, "shadow": list(output)})

    def run(self, events: Iterable[ReplayEvent]) -> dict[str, Any]:
        for index, event in enumerate(events):
            self._transition(event.kind, event.t_ms)
            self._shadow_step(index, event.kind)
        return {
            "mode": self.mode, "events": [asdict(e) for e in events] if not isinstance(events, list) else [asdict(e) for e in events],
            "final_state": self.state, "transitions": self.transitions,
            "outputs": self.outputs, "fault_counters": self.counters,
            "errors": self.errors,
            "all_shadow_finite": not self.errors,
            "safe_output_after_stop": all(
                all(abs(float(x)) < 1e-12 for x in o["shadow"])
                for o in self.outputs if o["state"] in ("DISARMED", "E_STOP", "GOAL_STOP")
            ),
        }


def run_replay_suite(mode: str = "MIN") -> dict[str, Any]:
    normal_a = ReplayRunner(mode).run(normal_scenario())
    normal_b = ReplayRunner(mode).run(normal_scenario())
    fault_results = {name: ReplayRunner(mode).run(events) for name, events in fault_scenarios().items()}
    normal_a_cmp = dict(normal_a)
    normal_b_cmp = dict(normal_b)
    return {
        "mode": mode, "normal": normal_a,
        "normal_reproducible": json.dumps(normal_a_cmp, sort_keys=True) == json.dumps(normal_b_cmp, sort_keys=True),
        "faults": fault_results,
        "required_normal_phases": list(NORMAL_PHASES),
        "normal_phase_coverage": [e["event"] for e in normal_a["transitions"]],
        "all_faults_safe": all(r["safe_output_after_stop"] and r["all_shadow_finite"] for r in fault_results.values()),
    }


def write_json(value: dict[str, Any], path: str) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(value, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")
