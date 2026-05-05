#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path


GREEN_LIMIT = 5e-6
YELLOW_LIMIT = 1e-2


@dataclass
class WaveletStats:
    ok_cases: int = 0
    error_cases: int = 0
    mse_approx: list[float] = field(default_factory=list)
    mse_detail: list[float] = field(default_factory=list)
    worst_mse: list[float] = field(default_factory=list)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Aggregate CPU accuracy CSV by wavelet and write a colored Markdown table."
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "accuracy.csv",
        help="Input accuracy CSV from run_accuracy.py.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "results" / "accuracy_summary.md",
        help="Output Markdown summary path.",
    )
    parser.add_argument(
        "--columns-per-row",
        type=int,
        default=3,
        help="Number of wavelet groups per Markdown table row, similar to wav/README.md.",
    )
    return parser.parse_args()


def natural_key(value: str) -> list[object]:
    return [int(part) if part.isdigit() else part for part in re.split(r"(\d+)", value)]


def mean(values: list[float]) -> float:
    return float(sum(values) / len(values)) if values else math.nan


def classify(value: float) -> str:
    if math.isnan(value):
        return "red"
    if value < GREEN_LIMIT:
        return "green"
    if value < YELLOW_LIMIT:
        return "yellow"
    return "red"


def colored(value: float) -> str:
    if math.isnan(value):
        return r"$${\color{red} \text{nan}}$$"
    color = classify(value)
    return rf"$${{\color{{{color}}} \text{{{value:.2e}}}}}$$"


def load_stats(path: Path) -> dict[str, WaveletStats]:
    stats: dict[str, WaveletStats] = defaultdict(WaveletStats)

    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            wavelet = row["wavelet"]
            item = stats[wavelet]

            if row["status"] != "ok":
                item.error_cases += 1
                continue

            mse_approx = float(row["mse_approx"])
            mse_detail = float(row["mse_detail"])
            item.ok_cases += 1
            item.mse_approx.append(mse_approx)
            item.mse_detail.append(mse_detail)
            item.worst_mse.append(max(mse_approx, mse_detail))

    return dict(stats)


def build_rows(stats: dict[str, WaveletStats]) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for wavelet in sorted(stats, key=natural_key):
        item = stats[wavelet]
        avg_approx = mean(item.mse_approx)
        avg_detail = mean(item.mse_detail)
        avg_worst = mean(item.worst_mse)
        max_worst = max(item.worst_mse) if item.worst_mse else math.nan
        rows.append(
            {
                "wavelet": wavelet,
                "ok_cases": item.ok_cases,
                "error_cases": item.error_cases,
                "avg_approx": avg_approx,
                "avg_detail": avg_detail,
                "avg_worst": avg_worst,
                "max_worst": max_worst,
                "class": classify(avg_worst),
            }
        )
    return rows


def table_header(columns_per_row: int) -> str:
    labels: list[str] = []
    separators: list[str] = []
    for _ in range(columns_per_row):
        labels.extend(["Wavelet", "Avg cA MSE", "Avg cD MSE", "Avg worst MSE"])
        separators.extend(["---", "---", "---", "---"])
    return "| " + " | ".join(labels) + " |\n" + "| " + " | ".join(separators) + " |"


def table_body(rows: list[dict[str, object]], columns_per_row: int) -> str:
    lines: list[str] = []
    for start in range(0, len(rows), columns_per_row):
        chunk = rows[start : start + columns_per_row]
        cells: list[str] = []
        for row in chunk:
            cells.extend(
                [
                    str(row["wavelet"]),
                    colored(float(row["avg_approx"])),
                    colored(float(row["avg_detail"])),
                    colored(float(row["avg_worst"])),
                ]
            )
        missing = columns_per_row - len(chunk)
        cells.extend([""] * missing * 4)
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def write_summary(path: Path, rows: list[dict[str, object]], columns_per_row: int) -> None:
    counts = {"green": 0, "yellow": 0, "red": 0}
    for row in rows:
        counts[str(row["class"])] += 1

    ok_cases = sum(int(row["ok_cases"]) for row in rows)
    error_cases = sum(int(row["error_cases"]) for row in rows)

    content = [
        "# CPU Wavelet Accuracy Summary",
        "",
        "Averages are computed per wavelet across all saved signal kinds and lengths in `accuracy.csv`.",
        "",
        "Classification uses the same MSE thresholds as `wav/usage/test.py`:",
        "",
        "- `green`: `avg_worst_mse < 5e-6`",
        "- `yellow`: `avg_worst_mse < 1e-2`",
        "- `red`: otherwise",
        "",
        f"Wavelets: {len(rows)}",
        f"OK cases: {ok_cases}",
        f"Error cases: {error_cases}",
        f"Classes: green={counts['green']}, yellow={counts['yellow']}, red={counts['red']}",
        "",
        table_header(columns_per_row),
        table_body(rows, columns_per_row),
        "",
        "## Max Worst MSE",
        "",
        "| Wavelet | Max worst MSE | OK cases | Error cases |",
        "| --- | --- | --- | --- |",
    ]

    for row in rows:
        content.append(
            f"| {row['wavelet']} | {colored(float(row['max_worst']))} | {row['ok_cases']} | {row['error_cases']} |"
        )

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(content) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.columns_per_row <= 0:
        raise ValueError("--columns-per-row must be positive")
    if not args.input.exists():
        raise FileNotFoundError(f"Accuracy CSV not found: {args.input}")

    stats = load_stats(args.input)
    rows = build_rows(stats)
    write_summary(args.output, rows, args.columns_per_row)
    print(f"Wrote {len(rows)} wavelet summaries to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
