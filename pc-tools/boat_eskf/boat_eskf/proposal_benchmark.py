"""Deterministic host-side benchmark for the competition proposal.

This module is deliberately separate from the firmware control path.  It is a
reference/shadow implementation used to compare algorithm groups and to make
the replay inputs reproducible.  Timings are *host timings* and are never
reported as XIAO execution timings.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from math import atan2, cos, exp, hypot, isfinite, pi, sin, sqrt
import json
import statistics
import sys
import time
import tracemalloc
from typing import Any, Callable, Iterable

from .coordinates import ned
from .eskf import ShadowEskf
from .measurements import tof_quality
from .quaternion import wrap_pi


MODES = ("BASE", "MIN", "MID", "FULL", "LEGACY")

MIN_ALGORITHMS = (
    "gnss_convert", "waypoint_manager", "los_guidance", "launch_align",
    "cog_validity", "yaw_command", "roll_pd", "tof_select",
    "tof_tilt_correct", "tof_lowpass", "height_p", "wing_synthesis",
    "safety_state", "shadow_output",
)
MID_ALGORITHMS = MIN_ALGORITHMS + ("leaky_ilos", "yaw_rate_inner")
FULL_ALGORITHMS = MID_ALGORITHMS + ("horizontal_ekf5", "height_kf2")
BASE_ALGORITHMS = ("gnss_convert", "safety_state", "shadow_output")


def _finite(value: Any) -> bool:
    if isinstance(value, (float, int)):
        return isfinite(float(value))
    if isinstance(value, (tuple, list)):
        return all(_finite(x) for x in value)
    if isinstance(value, dict):
        return all(_finite(x) for x in value.values())
    return True


def _percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    x = sorted(values)
    if len(x) == 1:
        return x[0]
    rank = (len(x) - 1) * p
    lo, hi = int(rank), min(len(x) - 1, int(rank) + 1)
    return x[lo] + (x[hi] - x[lo]) * (rank - lo)


@dataclass
class OperationStats:
    name: str
    deadline_us: float
    samples_us: list[float] = field(default_factory=list)
    calls: int = 0
    total_us: float = 0.0
    minimum_us: float = float("inf")
    maximum_us: float = 0.0
    deadline_miss: int = 0
    nan_inf: int = 0
    invalid_inputs: int = 0
    saturation: int = 0
    memory_delta_bytes: int = 0

    def record(self, elapsed_us: float, output: Any, invalid: bool, memory_delta: int) -> None:
        self.calls += 1
        self.total_us += elapsed_us
        self.minimum_us = min(self.minimum_us, elapsed_us)
        self.maximum_us = max(self.maximum_us, elapsed_us)
        self.samples_us.append(elapsed_us)
        self.deadline_miss += int(elapsed_us > self.deadline_us)
        self.nan_inf += int(not _finite(output))
        self.invalid_inputs += int(invalid)
        self.memory_delta_bytes = max(self.memory_delta_bytes, max(0, memory_delta))

    def as_dict(self) -> dict[str, Any]:
        return {
            "calls": self.calls,
            "total_us": round(self.total_us, 3),
            "average_us": round(self.total_us / self.calls, 3) if self.calls else 0.0,
            "min_us": round(self.minimum_us if self.calls else 0.0, 3),
            "max_us": round(self.maximum_us, 3),
            "p50_us": round(_percentile(self.samples_us, .50), 3),
            "p95_us": round(_percentile(self.samples_us, .95), 3),
            "p99_us": round(_percentile(self.samples_us, .99), 3),
            "deadline_us": self.deadline_us,
            "deadline_miss": self.deadline_miss,
            "nan_inf": self.nan_inf,
            "invalid_inputs": self.invalid_inputs,
            "output_saturation": self.saturation,
            "memory_delta_bytes_max": self.memory_delta_bytes,
        }


@dataclass(frozen=True)
class Sample:
    index: int
    t_us: int
    lat: float
    lon: float
    speed_mps: float
    course_rad: float
    yaw_rad: float
    yaw_rate: float
    roll_rad: float
    pitch_rad: float
    tof_ranges_mm: tuple[int, ...]
    tof_status: tuple[int, ...]
    bno_valid: bool = True
    gnss_valid: bool = True


def deterministic_sample(index: int, hz: int = 100) -> Sample:
    """Return the same bounded input for a given index on every host."""
    t_us = index * (1_000_000 // hz)
    phase = index * 0.017
    lat = 35.000000 + index * 0.000001
    lon = 139.000000 + 0.00002 * sin(phase)
    speed = 1.15 + 0.08 * sin(phase * .7)
    course = 0.35 + 0.03 * sin(phase)
    yaw = course + 0.02 * sin(phase * .5)
    yaw_rate = 0.02 * cos(phase)
    roll = 0.03 * sin(phase * .4)
    pitch = 0.02 * cos(phase * .3)
    ranges = tuple(1200 + int(35 * sin(phase + z * .23)) for z in range(64))
    statuses = (5,) * 64
    return Sample(index, t_us, lat, lon, speed, course, yaw, yaw_rate,
                  roll, pitch, ranges, statuses)


@dataclass
class _State:
    waypoint_index: int = 0
    height_m: float = 1.2
    height_rate: float = 0.0
    ilos_integral: float = 0.0
    yaw_rate_bias: float = 0.0
    horizontal_x: float = 0.0
    horizontal_y: float = 0.0
    height_kf: float = 1.2
    safety: str = "DISARMED"
    command_left: float = 0.0
    command_right: float = 0.0
    command_yaw: float = 0.0


class ProposalAlgorithms:
    """Small, deterministic, actuator-disconnected proposal reference."""

    DEADLINES_US = {
        "gnss_convert": 100_000, "waypoint_manager": 100_000,
        "los_guidance": 50_000, "launch_align": 20_000,
        "cog_validity": 20_000, "yaw_command": 20_000,
        "roll_pd": 20_000, "tof_select": 100_000,
        "tof_tilt_correct": 20_000, "tof_lowpass": 20_000,
        "height_p": 20_000, "wing_synthesis": 20_000,
        "safety_state": 20_000, "shadow_output": 20_000,
        "leaky_ilos": 50_000, "yaw_rate_inner": 20_000,
        "horizontal_ekf5": 50_000, "height_kf2": 50_000,
        "legacy_eskf": 50_000,
    }

    def __init__(self, mode: str):
        mode = mode.upper()
        if mode not in MODES:
            raise ValueError(f"unknown mode: {mode}")
        self.mode = mode
        self.state = _State()
        self.origin = (35.0, 139.0, 0.0)
        self.waypoints = ((2.0, 0.0), (5.0, 1.0), (7.0, -1.0))
        self.metrics = {name: OperationStats(name, self.DEADLINES_US[name])
                        for name in self.enabled_algorithms}
        self._eskf = ShadowEskf() if mode == "LEGACY" else None
        self._trace_memory = False

    @property
    def enabled_algorithms(self) -> tuple[str, ...]:
        if self.mode == "BASE":
            return BASE_ALGORITHMS
        if self.mode == "MIN":
            return MIN_ALGORITHMS
        if self.mode == "MID":
            return MID_ALGORITHMS
        if self.mode == "FULL":
            return FULL_ALGORITHMS
        return ("legacy_eskf",)

    def _call(self, name: str, fn: Callable[[], Any], *inputs: Any) -> Any:
        stat = self.metrics[name]
        invalid = not _finite(inputs)
        before = tracemalloc.get_traced_memory()[0] if self._trace_memory else 0
        start = time.perf_counter_ns()
        try:
            result = fn()
        except (ArithmeticError, ValueError, IndexError):
            result = float("nan")
        elapsed = (time.perf_counter_ns() - start) / 1000.0
        after = tracemalloc.get_traced_memory()[0] if self._trace_memory else 0
        stat.record(elapsed, result, invalid, after - before)
        return result

    def gnss_convert(self, s: Sample) -> tuple[float, float, float]:
        return self._call("gnss_convert", lambda: ned(s.lat, s.lon, 0.0, self.origin), s.lat, s.lon)

    def waypoint_manager(self, ned_pos: tuple[float, float, float]) -> tuple[float, float, bool]:
        def run():
            target = self.waypoints[min(self.state.waypoint_index, len(self.waypoints) - 1)]
            d = hypot(target[0] - ned_pos[0], target[1] - ned_pos[1])
            if d < .5 and self.state.waypoint_index < len(self.waypoints) - 1:
                self.state.waypoint_index += 1
                target = self.waypoints[self.state.waypoint_index]
                d = hypot(target[0] - ned_pos[0], target[1] - ned_pos[1])
            return d, atan2(target[1] - ned_pos[1], target[0] - ned_pos[0]), d < .5 and self.state.waypoint_index == len(self.waypoints) - 1
        return self._call("waypoint_manager", run, ned_pos)

    def los_guidance(self, ned_pos: tuple[float, float, float], target_bearing: float) -> float:
        return self._call("los_guidance", lambda: wrap_pi(target_bearing - atan2(ned_pos[1], max(.001, ned_pos[0]))), ned_pos, target_bearing)

    def launch_align(self, yaw: float, target: float) -> float:
        return self._call("launch_align", lambda: max(-pi, min(pi, wrap_pi(target - yaw))), yaw, target)

    def cog_validity(self, speed: float, course: float, age_ms: float = 0.0) -> bool:
        return self._call("cog_validity", lambda: bool(isfinite(speed) and isfinite(course) and speed >= 0.5 and age_ms <= 500.0), speed, course, age_ms)

    def yaw_command(self, yaw_error: float) -> float:
        def run():
            out = max(-1.0, min(1.0, 0.8 * yaw_error))
            if abs(out) >= 1.0:
                self.metrics["yaw_command"].saturation += 1
            return out
        return self._call("yaw_command", run, yaw_error)

    def roll_pd(self, roll: float, roll_rate: float) -> float:
        def run():
            out = max(-1.0, min(1.0, -1.4 * roll - .25 * roll_rate))
            if abs(out) >= 1.0:
                self.metrics["roll_pd"].saturation += 1
            return out
        return self._call("roll_pd", run, roll, roll_rate)

    def tof_select(self, s: Sample) -> tuple[int, float, float]:
        return self._call("tof_select", lambda: tof_quality(s.tof_ranges_mm, s.tof_status), s.tof_ranges_mm, s.tof_status)

    def tof_tilt_correct(self, distance: float, roll: float, pitch: float) -> float:
        return self._call("tof_tilt_correct", lambda: distance * cos(roll) * cos(pitch), distance, roll, pitch)

    def tof_lowpass(self, distance: float) -> float:
        def run():
            self.state.height_m = .9 * self.state.height_m + .1 * distance
            return self.state.height_m
        return self._call("tof_lowpass", run, distance)

    def height_p(self, height: float) -> float:
        return self._call("height_p", lambda: max(-1.0, min(1.0, .7 * (1.2 - height))), height)

    def wing_synthesis(self, roll_cmd: float, yaw_cmd: float) -> tuple[float, float]:
        def run():
            left = max(-1.0, min(1.0, roll_cmd + yaw_cmd))
            right = max(-1.0, min(1.0, -roll_cmd + yaw_cmd))
            if abs(left) >= 1.0 or abs(right) >= 1.0:
                self.metrics["wing_synthesis"].saturation += 1
            return left, right
        return self._call("wing_synthesis", run, roll_cmd, yaw_cmd)

    def safety_state(self, event: str) -> str:
        def run():
            if event in ("STOP", "E_STOP", "HEARTBEAT_TIMEOUT", "FAULT"):
                self.state.safety = "E_STOP" if event == "E_STOP" else "DISARMED"
            elif event == "START" and self.state.safety == "DISARMED":
                self.state.safety = "RUNNING"
            return self.state.safety
        return self._call("safety_state", run, event)

    def shadow_output(self, left: float, right: float, yaw: float) -> tuple[float, float, float]:
        def run():
            # Explicit double guard: this is a report-only value, never PWM/VESC.
            if self.state.safety != "RUNNING":
                return (0.0, 0.0, 0.0)
            return tuple(max(-1.0, min(1.0, float(x))) for x in (left, right, yaw))
        return self._call("shadow_output", run, left, right, yaw)

    def leaky_ilos(self, cross_track: float) -> float:
        def run():
            self.state.ilos_integral = .995 * self.state.ilos_integral + .02 * cross_track
            return self.state.ilos_integral
        return self._call("leaky_ilos", run, cross_track)

    def yaw_rate_inner(self, desired: float, measured: float) -> float:
        return self._call("yaw_rate_inner", lambda: max(-1.0, min(1.0, .5 * (desired - measured))), desired, measured)

    def horizontal_ekf5(self, pos: tuple[float, float, float], speed: float) -> tuple[float, float]:
        def run():
            self.state.horizontal_x = .98 * self.state.horizontal_x + .02 * (pos[0] + speed)
            self.state.horizontal_y = .98 * self.state.horizontal_y + .02 * pos[1]
            return self.state.horizontal_x, self.state.horizontal_y
        return self._call("horizontal_ekf5", run, pos, speed)

    def height_kf2(self, distance: float) -> float:
        def run():
            self.state.height_kf = .97 * self.state.height_kf + .03 * distance
            return self.state.height_kf
        return self._call("height_kf2", run, distance)

    def legacy_eskf(self, s: Sample) -> bool:
        return self._call("legacy_eskf", lambda: self._eskf.predict(s.t_us, (0.0, 0.0, s.yaw_rate), (0.0, 0.0, -9.80665)), s.t_us)

    def execute(self, s: Sample) -> tuple[float, float, float]:
        enabled = set(self.enabled_algorithms)
        pos = self.gnss_convert(s) if "gnss_convert" in enabled else (0.0, 0.0, 0.0)
        if "legacy_eskf" in enabled:
            self.legacy_eskf(s)
            return (0.0, 0.0, 0.0)
        if "waypoint_manager" in enabled:
            _, bearing, goal = self.waypoint_manager(pos)
        else:
            bearing, goal = s.course_rad, False
        los = self.los_guidance(pos, bearing) if "los_guidance" in enabled else 0.0
        align = self.launch_align(s.yaw_rad, bearing) if "launch_align" in enabled else 0.0
        cog_ok = self.cog_validity(s.speed_mps, s.course_rad) if "cog_validity" in enabled else True
        yaw = self.yaw_command(los + align) if "yaw_command" in enabled else 0.0
        if "yaw_rate_inner" in enabled:
            yaw += self.yaw_rate_inner(yaw, s.yaw_rate)
        if "leaky_ilos" in enabled:
            yaw += self.leaky_ilos(pos[1])
        roll = self.roll_pd(s.roll_rad, 0.0) if "roll_pd" in enabled else 0.0
        if "tof_select" in enabled:
            zones, distance, _ = self.tof_select(s)
            if zones == 0:
                distance = self.state.height_m
        else:
            distance = self.state.height_m
        if "tof_tilt_correct" in enabled:
            distance = self.tof_tilt_correct(distance, s.roll_rad, s.pitch_rad)
        if "tof_lowpass" in enabled:
            distance = self.tof_lowpass(distance)
        height = self.height_p(distance) if "height_p" in enabled else 0.0
        if "height_kf2" in enabled:
            height += .1 * (self.height_kf2(distance) - distance)
        if "horizontal_ekf5" in enabled:
            self.horizontal_ekf5(pos, s.speed_mps)
        if "wing_synthesis" in enabled:
            left, right = self.wing_synthesis(roll + height, yaw)
        else:
            left, right = 0.0, 0.0
        event = "START" if s.index == 0 else ("GOAL" if goal else "NONE")
        safety = self.safety_state(event) if "safety_state" in enabled else "RUNNING"
        if not cog_ok or not s.bno_valid or not s.gnss_valid:
            safety = self.safety_state("FAULT") if "safety_state" in enabled else "DISARMED"
        return self.shadow_output(left, right, yaw) if "shadow_output" in enabled else (0.0, 0.0, 0.0)


def run_benchmark(mode: str, iterations: int = 10_000) -> dict[str, Any]:
    if iterations < 10_000:
        raise ValueError("fixed benchmark requires at least 10000 iterations")
    runner = ProposalAlgorithms(mode)
    tracemalloc.start()
    runner._trace_memory = True
    outputs: list[tuple[float, float, float]] = []
    for i in range(iterations):
        outputs.append(runner.execute(deterministic_sample(i)))
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    finite_outputs = sum(_finite(x) for x in outputs)
    return {
        "mode": mode.upper(), "iterations": iterations,
        "host_timing_only": True,
        "algorithm_set": list(runner.enabled_algorithms),
        "metrics": {name: metric.as_dict() for name, metric in runner.metrics.items()},
        "output_count": len(outputs), "finite_output_count": finite_outputs,
        "output_saturation_total": sum(m.saturation for m in runner.metrics.values()),
        "tracemalloc_peak_bytes": peak, "tracemalloc_current_bytes": current,
        "deterministic_input": "deterministic_sample(index, 100Hz)",
    }


def run_all_benchmarks(iterations: int = 10_000) -> dict[str, Any]:
    return {mode: run_benchmark(mode, iterations) for mode in MODES}


def write_json(value: dict[str, Any], path: str) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(value, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")
