#!/usr/bin/env python3
"""Run the host-only proposal benchmark and deterministic replay suite."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from boat_eskf.proposal_benchmark import MODES, run_all_benchmarks, run_benchmark, write_json as write_benchmark
from boat_eskf.proposal_replay import run_replay_suite, write_json as write_replay


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--iterations", type=int, default=10_000)
    p.add_argument("--mode", choices=("ALL",) + MODES, default="ALL")
    p.add_argument("--out-dir", type=Path, default=ROOT / "results" / "proposal_20260804")
    args = p.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    benchmarks = run_all_benchmarks(args.iterations) if args.mode == "ALL" else {args.mode: run_benchmark(args.mode, args.iterations)}
    benchmark_path = args.out_dir / "benchmark.json"
    write_benchmark(benchmarks, str(benchmark_path))
    replay = {mode: run_replay_suite(mode) for mode in (MODES if args.mode == "ALL" else (args.mode,))}
    replay_path = args.out_dir / "replay.json"
    write_replay(replay, str(replay_path))
    summary = {
        "iterations": args.iterations,
        "host_timing_only": True,
        "benchmark_path": str(benchmark_path),
        "replay_path": str(replay_path),
        "modes": list(benchmarks),
        "all_benchmark_outputs_finite": all(v["finite_output_count"] == v["output_count"] for v in benchmarks.values()),
        "all_replay_deterministic": all(v["normal_reproducible"] and v["all_faults_safe"] for v in replay.values()),
    }
    with open(args.out_dir / "summary.json", "w", encoding="utf-8", newline="\n") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write("\n")
    print(json.dumps(summary, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if summary["all_benchmark_outputs_finite"] and summary["all_replay_deterministic"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
