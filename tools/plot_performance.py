#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot CPU wavelet performance CSV.")
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "performance.csv",
        help="Input performance CSV from run_performance.py.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "performance_plots",
        help="Output plot directory.",
    )
    parser.add_argument("--format", choices=["svg", "png"], default="svg", help="Output plot format.")
    return parser.parse_args()


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def row_dimension(row: dict[str, str]) -> str:
    return row.get("dimension", "").strip()


def cpu_groups(rows: list[dict[str, str]]) -> dict[int, list[tuple[int, float]]]:
    groups: dict[int, list[tuple[int, float]]] = defaultdict(list)
    for row in rows:
        if not row["implementation"].startswith("cpu"):
            continue
        threads = int(row["threads"])
        groups[threads].append((int(row["size"]), float(row["mean_ms"])))
    for values in groups.values():
        values.sort()
    return dict(sorted(groups.items()))


def time_series_groups(rows: list[dict[str, str]]) -> dict[str, list[tuple[int, float]]]:
    groups: dict[str, list[tuple[int, float]]] = defaultdict(list)
    for row in rows:
        implementation = row["implementation"]
        dimension = row_dimension(row)
        if implementation.startswith("cpu"):
            label = f"CPU {dimension} {row['threads']} thread(s)" if dimension else f"CPU {row['threads']} thread(s)"
        elif implementation == "pywavelets":
            label = f"PyWavelets {dimension}" if dimension else "PyWavelets"
        else:
            label = f"{implementation} {dimension}".strip()
        groups[label].append((int(row["size"]), float(row["mean_ms"])))

    for values in groups.values():
        values.sort()
    return dict(sorted(groups.items()))


def savefig(output_dir: Path, stem: str, fmt: str) -> None:
    path = output_dir / f"{stem}.{fmt}"
    if fmt == "png":
        import matplotlib.pyplot as plt

        plt.savefig(path, dpi=160)
    else:
        import matplotlib.pyplot as plt

        plt.savefig(path, format="svg")
    print(f"Wrote {path}")


def main() -> int:
    args = parse_args()
    rows = load_rows(args.input)

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ModuleNotFoundError as error:
        raise SystemExit("matplotlib is required for plotting. Install it with `pip install matplotlib`.") from error

    args.output_dir.mkdir(parents=True, exist_ok=True)

    cpu = cpu_groups(rows)
    series = time_series_groups(rows)

    plt.figure(figsize=(10, 6))
    for label, values in series.items():
        linestyle = "--" if label.startswith("PyWavelets") else "-"
        plt.plot([x for x, _ in values], [y for _, y in values], label=label, linestyle=linestyle)
    plt.xlabel("Signal size")
    plt.ylabel("Mean time (ms)")
    plt.title("DWT Mean Runtime")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    savefig(args.output_dir, "total_time", args.format)
    plt.close()

    if 1 in cpu:
        base = dict(cpu[1])

        plt.figure(figsize=(10, 6))
        plotted = False
        for threads, values in cpu.items():
            if threads == 1:
                continue
            xs: list[int] = []
            ys: list[float] = []
            for size, mean_ms in values:
                if size in base and mean_ms > 0.0:
                    xs.append(size)
                    ys.append(base[size] / mean_ms)
            if xs:
                plt.plot(xs, ys, label=f"{threads} threads")
                plotted = True
        plt.xlabel("Signal size")
        plt.ylabel("Speedup vs 1 CPU thread")
        plt.title("CPU Speedup")
        plt.grid(True, alpha=0.3)
        if plotted:
            plt.legend()
            plt.tight_layout()
            savefig(args.output_dir, "speedup", args.format)
        plt.close()

        plt.figure(figsize=(10, 6))
        plotted = False
        for threads, values in cpu.items():
            if threads == 1:
                continue
            xs = []
            ys = []
            for size, mean_ms in values:
                if size in base and mean_ms > 0.0:
                    xs.append(size)
                    ys.append((base[size] / mean_ms) / float(threads))
            if xs:
                plt.plot(xs, ys, label=f"{threads} threads")
                plotted = True
        plt.xlabel("Signal size")
        plt.ylabel("Efficiency = speedup / threads")
        plt.title("CPU Parallel Efficiency")
        plt.grid(True, alpha=0.3)
        if plotted:
            plt.legend()
            plt.tight_layout()
            savefig(args.output_dir, "efficiency", args.format)
        plt.close()

    print(f"Wrote plots to {args.output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
