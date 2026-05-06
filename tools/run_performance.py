#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import subprocess
import time
from pathlib import Path
from typing import Any

import numpy as np


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
    parser = argparse.ArgumentParser(
        description="Run repeatable performance measurements for CPU lifting and PyWavelets."
    )
    parser.add_argument(
        "--cpu-bin",
        type=Path,
        default=default_cpu_bin(),
        help="Path to cpu_wavelet_bench executable.",
    )
    parser.add_argument(
        "--scheme",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "lifting_schemes" / "bior1.3.json",
        help="Lifting scheme JSON for CPU benchmark.",
    )
    parser.add_argument("--wavelet", default=None, help="PyWavelets wavelet name. Defaults to scheme stem.")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "performance.csv",
        help="Output CSV path.",
    )
    parser.add_argument("--kind", default="ramp", help="Signal kind.")
    parser.add_argument("--start", type=int, default=100_000, help="First signal size.")
    parser.add_argument("--end", type=int, default=1_000_000, help="Last signal size, inclusive.")
    parser.add_argument("--step", type=int, default=10_000, help="Signal size step.")
    parser.add_argument("--repeats", type=int, default=5, help="Timed repeats.")
    parser.add_argument("--warmups", type=int, default=1, help="Untimed warmups.")
    parser.add_argument("--dtype", choices=["float", "double"], default="float", help="CPU dtype.")
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8], help="CPU thread counts to test.")
    parser.add_argument("--parallel-threshold", type=int, default=32768, help="CPU stencil/split executor threshold.")
    parser.add_argument("--scale-threshold", type=int, default=131072, help="CPU scale executor threshold.")
    parser.add_argument("--skip-pywt", action="store_true", help="Do not run PyWavelets timings.")
    parser.add_argument("--append", action="store_true", help="Append to output CSV instead of replacing it.")
    return parser.parse_args()


def load_pywt() -> Any:
    try:
        import pywt  # type: ignore[import-not-found]
    except ModuleNotFoundError as error:
        raise SystemExit("PyWavelets is required unless --skip-pywt is used.") from error
    return pywt


def generate_signal(kind: str, length: int, magnitude: float = 1.0, seed: int = 2) -> np.ndarray:
    if kind == "uniform":
        return np.random.default_rng(seed).uniform(-magnitude, magnitude, size=length).astype(np.float32)
    if kind == "normal":
        return np.random.default_rng(seed).normal(0.0, magnitude, size=length).astype(np.float32)
    if kind == "alternating":
        out = np.ones(length, dtype=np.float32) * np.float32(magnitude)
        out[1::2] *= np.float32(-1.0)
        return out
    if kind == "impulse":
        out = np.zeros(length, dtype=np.float32)
        out[length // 2] = np.float32(magnitude)
        return out
    if kind == "ramp":
        return np.linspace(-magnitude, magnitude, num=length, dtype=np.float32)
    if kind == "sinusoidal":
        n = np.arange(length, dtype=np.float32)
        return (np.float32(magnitude) * np.sin(np.float32(2.0 * np.pi * 3.0) * n / np.float32(length))).astype(
            np.float32
        )
    if kind == "constant":
        return np.full(length, magnitude, dtype=np.float32)
    if kind == "step":
        out = np.zeros(length, dtype=np.float32)
        out[length // 2 :] = np.float32(magnitude)
        return out
    if kind == "square":
        n = np.arange(length, dtype=np.float32)
        phase = np.sin(np.float32(2.0 * np.pi * 3.0) * n / np.float32(length))
        return np.where(phase >= 0.0, np.float32(magnitude), np.float32(-magnitude)).astype(np.float32)
    raise ValueError(f"Unsupported signal kind: {kind}")


def run_cpu(args: argparse.Namespace, threads: int) -> list[dict[str, str]]:
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
    return rows


def time_pywt(args: argparse.Namespace) -> list[dict[str, str]]:
    pywt = load_pywt()
    wavelet_name = args.wavelet or args.scheme.stem
    wavelet = pywt.Wavelet(wavelet_name)
    rows: list[dict[str, str]] = []

    for size in range(args.start, args.end + 1, args.step):
        signal = generate_signal(args.kind, size)

        checksum = 0.0
        for _ in range(args.warmups):
            c_a, c_d = pywt.dwt(signal, wavelet=wavelet, mode="symmetric")
            checksum += float(np.sum(c_a) + np.sum(c_d))

        times_ms: list[float] = []
        for _ in range(args.repeats):
            start = time.perf_counter()
            c_a, c_d = pywt.dwt(signal, wavelet=wavelet, mode="symmetric")
            end = time.perf_counter()
            checksum += float(np.sum(c_a) + np.sum(c_d))
            times_ms.append((end - start) * 1000.0)

        rows.append(
            {
                "implementation": "pywavelets",
                "wavelet": wavelet_name,
                "signal_kind": args.kind,
                "size": str(size),
                "dtype": "float32",
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

    for threads in args.threads:
        print(f"Running CPU benchmark: threads={threads}")
        all_rows.extend(run_cpu(args, threads))

    if not args.skip_pywt:
        print("Running PyWavelets benchmark")
        all_rows.extend(time_pywt(args))

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
        writer.writerows(all_rows)

    print(f"Wrote {len(all_rows)} rows to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
