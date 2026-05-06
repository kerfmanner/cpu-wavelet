#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import subprocess
import time
from pathlib import Path

import numpy as np

from run_performance import default_cpu_bin, generate_signal, load_pywt


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description="Benchmark CPU wavelet on 1D signals against PyWavelets dwt2 on 2D signals with the same total size."
    )
    parser.add_argument("--cpu-bin", type=Path, default=default_cpu_bin(), help="Path to cpu_wavelet_bench.")
    parser.add_argument(
        "--scheme",
        type=Path,
        default=root / "lifting_schemes" / "bior1.3.json",
        help="Lifting scheme JSON for the CPU 1D benchmark.",
    )
    parser.add_argument("--wavelet", default=None, help="PyWavelets wavelet name. Defaults to scheme stem.")
    parser.add_argument(
        "--output",
        type=Path,
        default=root / "tests" / "results" / "pywt2d_cpu1d_performance.csv",
        help="Output CSV path.",
    )
    parser.add_argument("--kind", default="ramp", help="Signal kind.")
    parser.add_argument("--start", type=int, default=100_000, help="First total signal size.")
    parser.add_argument("--end", type=int, default=1_000_000, help="Last total signal size, inclusive.")
    parser.add_argument("--step", type=int, default=10_000, help="Total signal size step.")
    parser.add_argument(
        "--rows",
        type=int,
        default=100,
        help="Number of rows for PyWavelets 2D input. Each size must be divisible by rows.",
    )
    parser.add_argument("--repeats", type=int, default=5, help="Timed repeats.")
    parser.add_argument("--warmups", type=int, default=1, help="Untimed warmups.")
    parser.add_argument("--dtype", choices=["float", "double"], default="float", help="CPU dtype.")
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8], help="CPU thread counts to test.")
    parser.add_argument("--parallel-threshold", type=int, default=32768, help="CPU stencil/split executor threshold.")
    parser.add_argument("--scale-threshold", type=int, default=131072, help="CPU scale executor threshold.")
    parser.add_argument("--skip-cpu", action="store_true", help="Only run the PyWavelets 2D benchmark.")
    parser.add_argument("--skip-pywt", action="store_true", help="Only run the CPU 1D benchmark.")
    parser.add_argument("--append", action="store_true", help="Append to output CSV instead of replacing it.")
    return parser.parse_args()


def run_cpu_1d(args: argparse.Namespace, threads: int) -> list[dict[str, str]]:
    if not args.cpu_bin.exists():
        raise FileNotFoundError(f"CPU benchmark binary not found: {args.cpu_bin}")

    completed = subprocess.run(
        [
            str(args.cpu_bin),
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
            "--repeats",
            str(args.repeats),
            "--warmups",
            str(args.warmups),
            "--dtype",
            args.dtype,
            "--policy",
            "openmp",
            "--threads",
            str(threads),
            "--parallel-threshold",
            str(args.parallel_threshold),
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
        row["implementation"] = "cpu"
        row["dimension"] = "1d"
        row["rows"] = ""
        row["cols"] = row["size"]
    return rows


def reshape_2d(signal: np.ndarray, rows: int, total_size: int) -> np.ndarray:
    if total_size % rows != 0:
        raise ValueError(f"size {total_size} is not divisible by --rows {rows}")
    return signal.reshape(rows, total_size // rows)


def time_pywt_2d(args: argparse.Namespace) -> list[dict[str, str]]:
    pywt = load_pywt()
    wavelet_name = args.wavelet or args.scheme.stem
    wavelet = pywt.Wavelet(wavelet_name)
    rows: list[dict[str, str]] = []
    numpy_dtype = np.float64 if args.dtype == "double" else np.float32

    for size in range(args.start, args.end + 1, args.step):
        signal_1d = generate_signal(args.kind, size).astype(numpy_dtype, copy=False)
        signal_2d = reshape_2d(signal_1d, args.rows, size)
        cols = signal_2d.shape[1]

        checksum = 0.0
        for _ in range(args.warmups):
            c_a, (c_h, c_v, c_d) = pywt.dwt2(signal_2d, wavelet=wavelet, mode="symmetric")
            checksum += float(np.sum(c_a) + np.sum(c_h) + np.sum(c_v) + np.sum(c_d))

        times_ms: list[float] = []
        for _ in range(args.repeats):
            start = time.perf_counter()
            c_a, (c_h, c_v, c_d) = pywt.dwt2(signal_2d, wavelet=wavelet, mode="symmetric")
            end = time.perf_counter()
            checksum += float(np.sum(c_a) + np.sum(c_h) + np.sum(c_v) + np.sum(c_d))
            times_ms.append((end - start) * 1000.0)

        rows.append(
            {
                "implementation": "pywavelets",
                "dimension": "2d",
                "wavelet": wavelet_name,
                "signal_kind": args.kind,
                "size": str(size),
                "rows": str(args.rows),
                "cols": str(cols),
                "dtype": "float64" if numpy_dtype == np.float64 else "float32",
                "policy": "native",
                "threads": "0",
                "parallel_threshold": "",
                "scale_threshold": "",
                "repeats": str(args.repeats),
                "warmups": str(args.warmups),
                "mean_ms": f"{sum(times_ms) / len(times_ms):.10g}",
                "min_ms": f"{min(times_ms):.10g}",
                "max_ms": f"{max(times_ms):.10g}",
                "checksum": f"{checksum:.10g}",
            }
        )

    return rows


def main() -> int:
    args = parse_args()
    all_rows: list[dict[str, str]] = []

    if not args.skip_cpu:
        for threads in args.threads:
            print(f"Running CPU 1D benchmark: threads={threads}")
            all_rows.extend(run_cpu_1d(args, threads))

    if not args.skip_pywt:
        print(f"Running PyWavelets 2D benchmark: rows={args.rows}")
        all_rows.extend(time_pywt_2d(args))

    fieldnames = [
        "implementation",
        "dimension",
        "wavelet",
        "signal_kind",
        "size",
        "rows",
        "cols",
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
        writer.writerows(all_rows)

    print(f"Wrote {len(all_rows)} rows to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
