#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import subprocess
from pathlib import Path


def default_batch_bin() -> Path:
    env_path = os.environ.get("CPU_WAVELET_BATCH_BENCH")
    if env_path:
        return Path(env_path)

    root = Path(__file__).resolve().parents[1]
    candidates = [
        root / "build" / "Release-openmp" / "cpu_wavelet_batch_bench",
        root / "build" / "Release-openmp" / "cpu_wavelet_batch_bench.exe",
        root / "build" / "Release" / "cpu_wavelet_batch_bench",
        root / "build" / "Release" / "cpu_wavelet_batch_bench.exe",
        root / "build" / "cpu_wavelet_batch_bench",
        root / "build" / "cpu_wavelet_batch_bench.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run batch-level CPU lifting benchmarks.")
    parser.add_argument("--batch-bin", type=Path, default=default_batch_bin(), help="Path to cpu_wavelet_batch_bench.")
    parser.add_argument(
        "--scheme",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "lifting_schemes" / "bior1.3.json",
        help="Lifting scheme JSON.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "batch_performance.csv",
        help="Output CSV path.",
    )
    parser.add_argument("--kind", default="ramp", help="Signal kind.")
    parser.add_argument("--start", type=int, default=1024, help="First per-signal size.")
    parser.add_argument("--end", type=int, default=65536, help="Last per-signal size, inclusive.")
    parser.add_argument("--step", type=int, default=1024, help="Per-signal size step.")
    parser.add_argument("--batch-sizes", type=int, nargs="+", default=[8, 16, 32, 64], help="Batch sizes to test.")
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8], help="Outer batch thread counts.")
    parser.add_argument("--dtype", choices=["float", "double"], default="float", help="Scalar type.")
    parser.add_argument("--repeats", type=int, default=5, help="Timed repeats.")
    parser.add_argument("--warmups", type=int, default=1, help="Untimed warmups.")
    parser.add_argument("--stencil-threshold", type=int, default=32768, help="Inner stencil threshold.")
    parser.add_argument("--scale-threshold", type=int, default=131072, help="Inner scale threshold.")
    parser.add_argument("--batch-threshold", type=int, default=1, help="Outer batch threshold.")
    parser.add_argument("--inner-policy", choices=["serial", "openmp"], default="serial", help="Per-signal policy.")
    parser.add_argument("--inner-threads", type=int, default=0, help="Per-signal OpenMP threads.")
    parser.add_argument("--append", action="store_true", help="Append to output CSV instead of replacing it.")
    return parser.parse_args()


def run_one(args: argparse.Namespace, batch_size: int, threads: int) -> list[dict[str, str]]:
    if not args.batch_bin.exists():
        raise FileNotFoundError(f"Batch benchmark binary not found: {args.batch_bin}")

    completed = subprocess.run(
        [
            str(args.batch_bin),
            "--scheme",
            str(args.scheme),
            "--kind",
            args.kind,
            "--start",
            str(args.start),
            "--end",
            str(args.end),
            "--step",
            str(args.step),
            "--batch-size",
            str(batch_size),
            "--repeats",
            str(args.repeats),
            "--warmups",
            str(args.warmups),
            "--dtype",
            args.dtype,
            "--batch-policy",
            "openmp",
            "--batch-threads",
            str(threads),
            "--batch-threshold",
            str(args.batch_threshold),
            "--inner-policy",
            args.inner_policy,
            "--inner-threads",
            str(args.inner_threads),
            "--stencil-threshold",
            str(args.stencil_threshold),
            "--scale-threshold",
            str(args.scale_threshold),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())

    rows = list(csv.DictReader(completed.stdout.splitlines()))
    for row in rows:
        row["implementation"] = "cpu_batch"
    return rows


def main() -> int:
    args = parse_args()
    rows: list[dict[str, str]] = []

    for batch_size in args.batch_sizes:
        for threads in args.threads:
            print(f"Running batch benchmark: batch_size={batch_size}, threads={threads}")
            rows.extend(run_one(args, batch_size, threads))

    fieldnames = [
        "implementation",
        "wavelet",
        "signal_kind",
        "signal_size",
        "batch_size",
        "total_samples",
        "dtype",
        "batch_policy",
        "batch_threads",
        "inner_policy",
        "inner_threads",
        "parallel_threshold",
        "scale_threshold",
        "batch_threshold",
        "repeats",
        "warmups",
        "mean_ms",
        "min_ms",
        "max_ms",
        "checksum",
    ]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    mode = "a" if args.append and args.output.exists() else "w"
    with args.output.open(mode, encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        if mode == "w":
            writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} rows to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
