#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from typing import Any


DEFAULT_LENGTHS = [8, 15, 17, 33, 64, 65, 128]
DEFAULT_MAGNITUDE = 1.0
DEFAULT_SEED = 2


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_ttnn_signal_module() -> Any:
    path = repo_root() / "tt-wavelet" / "ttnn-wavelet" / "tt_wavelet" / "signal.py"
    spec = importlib.util.spec_from_file_location("ttnn_wavelet_signal", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load signal module from {path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a repeatable short-signal dataset using ttnn-wavelet signal generators."
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "tests" / "data" / "signals_short.json",
        help="Output JSON path.",
    )
    parser.add_argument(
        "--lengths",
        type=int,
        nargs="+",
        default=DEFAULT_LENGTHS,
        help=f"Signal lengths to generate. Default: {DEFAULT_LENGTHS}",
    )
    parser.add_argument(
        "--magnitude",
        type=float,
        default=DEFAULT_MAGNITUDE,
        help="Signal magnitude passed into InputSpec.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=DEFAULT_SEED,
        help="Seed for stochastic signal kinds.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    signal_module = load_ttnn_signal_module()

    stochastic = {
        signal_module.SignalType.UNIFORM,
        signal_module.SignalType.NORMAL,
    }

    cases: list[dict[str, Any]] = []
    for length in args.lengths:
        for kind in signal_module.SignalType:
            seed = args.seed if kind in stochastic else None
            spec = signal_module.InputSpec(
                kind=kind,
                length=int(length),
                magnitude=float(args.magnitude),
                seed=seed,
            )
            values = signal_module.generate_signal(spec)
            cases.append(
                {
                    "kind": kind.name,
                    "length": int(length),
                    "magnitude": float(args.magnitude),
                    "seed": seed,
                    "values": [float(value) for value in values],
                }
            )

    payload = {
        "version": 1,
        "source": "tt-wavelet/ttnn-wavelet/tt_wavelet/signal.py",
        "cases": cases,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {len(cases)} signals to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
