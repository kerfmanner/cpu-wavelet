#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import subprocess
from pathlib import Path


def default_cpu_bin() -> Path:
    env_path = os.environ.get("CPU_WAVELET_BENCH")
    if env_path:
        return Path(env_path)

    root = Path(__file__).resolve().parents[1]
    candidates = [
        root / "build" / "Release-openmp" / "cpu_wavelet_bench",
        root / "build" / "Release-openmp" / "cpu_wavelet_bench.exe",
        root / "build" / "Release" / "cpu_wavelet_bench",
        root / "build" / "Release" / "cpu_wavelet_bench.exe",
        root / "build" / "cpu_wavelet_bench",
        root / "build" / "cpu_wavelet_bench.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Sweep stencil/scale parallel thresholds for CPU lifting.")
    parser.add_argument("--cpu-bin", type=Path, default=default_cpu_bin(), help="Path to cpu_wavelet_bench.")
    parser.add_argument(
        "--schemes",
        type=Path,
        nargs="+",
        default=[root / "lifting_schemes" / "bior1.3.json"],
        help="Scheme JSON files to test.",
    )
    parser.add_argument("--kinds", nargs="+", default=["ramp"], help="Signal kinds to test.")
    parser.add_argument("--sizes", type=int, nargs="+", default=[100_000, 250_000, 500_000, 1_000_000])
    parser.add_argument("--dtypes", choices=["float", "double"], nargs="+", default=["float"])
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument(
        "--stencil-thresholds",
        type=int,
        nargs="+",
        default=[8192, 32768, 131072, 524288],
        help="Stencil/split thresholds to test.",
    )
    parser.add_argument(
        "--scale-thresholds",
        type=int,
        nargs="+",
        default=[32768, 131072, 524288, 1000000000],
        help="Scale thresholds to test.",
    )
    parser.add_argument("--repeats", type=int, default=5, help="Timed repeats.")
    parser.add_argument("--warmups", type=int, default=1, help="Untimed warmups.")
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "tests" / "results" / "threshold_sweep.csv",
        help="Output CSV path.",
    )
    parser.add_argument("--append", action="store_true", help="Append to output CSV instead of replacing it.")
    return parser.parse_args()


def run_one(
    args: argparse.Namespace,
    scheme: Path,
    kind: str,
    size: int,
    dtype: str,
    threads: int,
    stencil_threshold: int,
    scale_threshold: int,
) -> list[dict[str, str]]:
    if not args.cpu_bin.exists():
        raise FileNotFoundError(f"CPU benchmark binary not found: {args.cpu_bin}")

    completed = subprocess.run(
        [
            str(args.cpu_bin),
            "--scheme",
            str(scheme),
            "--kind",
            kind,
            "--start",
            str(size),
            "--end",
            str(size),
            "--step",
            str(size),
            "--repeats",
            str(args.repeats),
            "--warmups",
            str(args.warmups),
            "--dtype",
            dtype,
            "--policy",
            "openmp",
            "--threads",
            str(threads),
            "--stencil-threshold",
            str(stencil_threshold),
            "--scale-threshold",
            str(scale_threshold),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or completed.stdout.strip())

    rows = list(csv.DictReader(completed.stdout.splitlines()))
    for row in rows:
        row["implementation"] = "cpu"
    return rows


def main() -> int:
    args = parse_args()
    rows: list[dict[str, str]] = []

    for scheme in args.schemes:
        for kind in args.kinds:
            for size in args.sizes:
                for dtype in args.dtypes:
                    for threads in args.threads:
                        for stencil_threshold in args.stencil_thresholds:
                            for scale_threshold in args.scale_thresholds:
                                print(
                                    "Running threshold sweep: "
                                    f"scheme={scheme.stem}, kind={kind}, size={size}, dtype={dtype}, "
                                    f"threads={threads}, stencil={stencil_threshold}, scale={scale_threshold}"
                                )
                                rows.extend(
                                    run_one(
                                        args,
                                        scheme,
                                        kind,
                                        size,
                                        dtype,
                                        threads,
                                        stencil_threshold,
                                        scale_threshold,
                                    )
                                )

    fieldnames = [
        "implementation",
        "wavelet",
        "signal_kind",
        "size",
        "dtype",
        "policy",
        "threads",
        "parallel_threshold",
        "scale_threshold",
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
