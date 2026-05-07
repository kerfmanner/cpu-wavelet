#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import os
import tempfile
from collections import defaultdict
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot wavelet timing CSV files.")
    parser.add_argument(
        "--input",
        type=Path,
        nargs="+",
        default=None,
        help="Input CSV file(s). Supports TT-wavelet timing CSVs and old CPU performance CSVs.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "performance_plots",
        help="Output plot directory.",
    )
    parser.add_argument(
        "--format",
        choices=("svg", "png", "pdf"),
        default="svg",
        help="Output image format.",
    )
    parser.add_argument(
        "--labels",
        nargs="*",
        default=None,
        help="Optional labels for input CSV files, same count as --input.",
    )
    parser.add_argument(
        "--linear",
        action="store_true",
        help="Use linear y-axis for runtime comparison instead of log y-axis.",
    )
    return parser.parse_args()


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def detect_format(rows: list[dict[str, str]]) -> str:
    if not rows:
        raise ValueError("CSV is empty")

    columns = set(rows[0].keys())

    if {"wavelet", "signal_length", "pywt_mean_s", "tt_wavelet_mean_s", "status"} <= columns:
        return "tt_wavelet"

    if {"implementation", "threads", "size", "mean_ms"} <= columns:
        return "cpu_performance"

    raise ValueError(f"Unsupported CSV format. Columns: {sorted(columns)}")


def ok_tt_rows(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    return [row for row in rows if row.get("status", "ok") == "ok"]


def tt_series(rows: list[dict[str, str]], column: str, wavelet: str | None = None) -> list[tuple[int, float]]:
    values: list[tuple[int, float]] = []
    for row in ok_tt_rows(rows):
        if wavelet is not None and row["wavelet"] != wavelet:
            continue
        value = row.get(column, "")
        if value == "" or value.lower() == "nan":
            continue
        values.append((int(row["signal_length"]), float(value)))
    values.sort()
    return values


def tt_wavelets(rows_by_file: list[list[dict[str, str]]]) -> list[str]:
    names = set()
    for rows in rows_by_file:
        for row in ok_tt_rows(rows):
            names.add(row["wavelet"])
    return sorted(names)


def cpu_groups(rows: list[dict[str, str]]) -> dict[int, list[tuple[int, float]]]:
    groups: dict[int, list[tuple[int, float]]] = defaultdict(list)
    for row in rows:
        if row["implementation"] != "cpu":
            continue
        threads = int(row["threads"])
        groups[threads].append((int(row["size"]), float(row["mean_ms"])))
    for values in groups.values():
        values.sort()
    return dict(sorted(groups.items()))


def pywt_cpu_series(rows: list[dict[str, str]]) -> list[tuple[int, float]]:
    values = [
        (int(row["size"]), float(row["mean_ms"]))
        for row in rows
        if row["implementation"] == "pywavelets"
    ]
    values.sort()
    return values


def setup_matplotlib() -> None:
    if "MPLCONFIGDIR" not in os.environ:
        config_dir = Path(tempfile.gettempdir()) / "cpu-wavelet-matplotlib"
        config_dir.mkdir(parents=True, exist_ok=True)
        os.environ["MPLCONFIGDIR"] = str(config_dir)

    try:
        import matplotlib

        matplotlib.use("Agg")
    except ModuleNotFoundError as error:
        raise SystemExit("matplotlib is required for plotting. Install it with `pip install matplotlib`.") from error


def save_current_plot(output_dir: Path, name: str, image_format: str) -> Path:
    import matplotlib.pyplot as plt

    output_path = output_dir / f"{name}.{image_format}"
    if image_format == "svg":
        plt.savefig(output_path, format=image_format, transparent=True, bbox_inches="tight")
    else:
        plt.savefig(output_path, format=image_format, dpi=180, bbox_inches="tight")
    plt.close()
    return output_path


def set_log_axis(axis: str) -> None:
    import matplotlib.pyplot as plt
    from matplotlib.ticker import LogFormatterMathtext, LogLocator, NullFormatter

    ax = plt.gca()
    if axis == "x":
        ax.set_xscale("log", base=10)
        target_axis = ax.xaxis
    elif axis == "y":
        ax.set_yscale("log", base=10)
        target_axis = ax.yaxis
    else:
        raise ValueError(f"Unsupported axis: {axis}")

    target_axis.set_major_locator(LogLocator(base=10.0))
    target_axis.set_major_formatter(LogFormatterMathtext(base=10.0))
    target_axis.set_minor_locator(LogLocator(base=10.0, subs=tuple(range(2, 10))))
    target_axis.set_minor_formatter(NullFormatter())


def plot_tt_wavelet(
    rows_by_file: list[list[dict[str, str]]],
    labels: list[str],
    output_dir: Path,
    image_format: str,
    log_y: bool,
) -> list[Path]:
    import matplotlib.pyplot as plt

    output_paths: list[Path] = []
    wavelets = tt_wavelets(rows_by_file)

    for wavelet in wavelets:
        # Individual timing CSVs vs PyWavelets from the same CSV.
        for file_index, (rows, label) in enumerate(zip(rows_by_file, labels), start=1):
            pywt = tt_series(rows, "pywt_mean_s", wavelet)
            tt = tt_series(rows, "tt_wavelet_mean_s", wavelet)

            plt.figure(figsize=(10, 6))

            if pywt:
                plt.plot(
                    [x for x, _ in pywt],
                    [y * 1000.0 for _, y in pywt],
                    label="PyWavelets",
                    linestyle="--",
                    marker="o",
                    markersize=3,
                )

            if tt:
                plt.plot(
                    [x for x, _ in tt],
                    [y * 1000.0 for _, y in tt],
                    label=label,
                    marker="o",
                    markersize=3,
                )

            set_log_axis("x")
            if log_y:
                set_log_axis("y")

            plt.xlabel("Signal length")
            plt.ylabel("Mean time (ms)")
            plt.title(f"{label} vs PyWavelets ({wavelet})")
            plt.grid(True, alpha=0.3, which="both")
            plt.legend()
            plt.tight_layout()

            output_paths.append(
                save_current_plot(
                    output_dir,
                    f"{wavelet}_timing{file_index}_vs_pywavelets_log",
                    image_format,
                )
            )

        # All TT-wavelet timing lines plus one PyWavelets line.
        plt.figure(figsize=(10, 6))

        pywt = tt_series(rows_by_file[0], "pywt_mean_s", wavelet)
        if pywt:
            plt.plot(
                [x for x, _ in pywt],
                [y * 1000.0 for _, y in pywt],
                label="PyWavelets",
                linestyle="--",
                marker="o",
                markersize=3,
            )

        for rows, label in zip(rows_by_file, labels):
            tt = tt_series(rows, "tt_wavelet_mean_s", wavelet)
            if tt:
                plt.plot(
                    [x for x, _ in tt],
                    [y * 1000.0 for _, y in tt],
                    label=label,
                    marker="o",
                    markersize=3,
                )

        set_log_axis("x")
        if log_y:
            set_log_axis("y")

        plt.xlabel("Signal length")
        plt.ylabel("Mean time (ms)")
        plt.title(f"All TT-wavelet timings vs PyWavelets ({wavelet})")
        plt.grid(True, alpha=0.3, which="both")
        plt.legend()
        plt.tight_layout()

        output_paths.append(
            save_current_plot(
                output_dir,
                f"{wavelet}_all_timings_vs_pywavelets_log",
                image_format,
            )
        )

    return output_paths


def plot_cpu_performance(rows: list[dict[str, str]], output_dir: Path, image_format: str) -> list[Path]:
    import matplotlib.pyplot as plt

    output_paths: list[Path] = []
    cpu = cpu_groups(rows)
    pywt = pywt_cpu_series(rows)

    plt.figure(figsize=(10, 6))
    for threads, values in cpu.items():
        plt.plot([x for x, _ in values], [y for _, y in values], label=f"CPU {threads} thread(s)")
    if pywt:
        plt.plot([x for x, _ in pywt], [y for _, y in pywt], label="PyWavelets", linestyle="--")
    set_log_axis("x")
    set_log_axis("y")
    plt.xlabel("Signal size")
    plt.ylabel("Mean time (ms)")
    plt.title("DWT Mean Runtime")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    output_paths.append(save_current_plot(output_dir, "total_time", image_format))

    if 1 in cpu:
        base = dict(cpu[1])

        plt.figure(figsize=(10, 6))
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
        set_log_axis("x")
        set_log_axis("y")
        plt.xlabel("Signal size")
        plt.ylabel("Speedup vs 1 CPU thread")
        plt.title("CPU Speedup")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        output_paths.append(save_current_plot(output_dir, "speedup", image_format))

        plt.figure(figsize=(10, 6))
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
        set_log_axis("x")
        set_log_axis("y")
        plt.xlabel("Signal size")
        plt.ylabel("Efficiency = speedup / threads")
        plt.title("CPU Parallel Efficiency")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.tight_layout()
        output_paths.append(save_current_plot(output_dir, "efficiency", image_format))

    return output_paths


def default_inputs() -> list[Path]:
    root = Path(__file__).resolve().parents[1]
    candidates = sorted((root / "tests" / "results").glob("tt_wavelet_timings*.csv"))
    if candidates:
        return candidates
    return [root / "tests" / "results" / "performance.csv"]


def main() -> int:
    args = parse_args()
    input_paths = args.input if args.input is not None else default_inputs()

    if args.labels is not None and len(args.labels) != len(input_paths):
        raise SystemExit("--labels must have the same number of values as --input files")

    labels = args.labels if args.labels is not None else [path.stem for path in input_paths]

    rows_by_file = [load_rows(path) for path in input_paths]
    formats = {detect_format(rows) for rows in rows_by_file}

    if len(formats) != 1:
        raise SystemExit(f"Do not mix CSV formats in one run: {sorted(formats)}")

    setup_matplotlib()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    csv_format = formats.pop()
    if csv_format == "tt_wavelet":
        outputs = plot_tt_wavelet(rows_by_file, labels, args.output_dir, args.format, not args.linear)
    else:
        if len(rows_by_file) != 1:
            raise SystemExit("Old CPU performance format supports one input CSV per run")
        outputs = plot_cpu_performance(rows_by_file[0], args.output_dir, args.format)

    for output in outputs:
        print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
