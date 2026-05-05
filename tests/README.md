# CPU Accuracy Tests

This directory contains repeatable test inputs and result outputs for the CPU lifting implementation.

## Generate Signals

Signals are generated from `tt-wavelet/ttnn-wavelet/tt_wavelet/signal.py`.

```bash
python cpu-wavelet/tools/generate_signals.py
```

Default output:

```text
cpu-wavelet/tests/data/signals_short.json
```

The default dataset contains all TTNN signal kinds for short lengths:

```text
8, 15, 17, 33, 64, 65, 128
```

## Build CLI

With CMake:

```bash
cmake -S cpu-wavelet -B cpu-wavelet/build
cmake --build cpu-wavelet/build --target cpu_wavelet_cli
```

On Ubuntu, the helper script wraps common optimized/debug builds:

```bash
cpu-wavelet/scripts/build.sh --release
cpu-wavelet/scripts/build.sh --debug
cpu-wavelet/scripts/build.sh --release --openmp -j 8
```

If CMake is not convenient, direct compile works too:

```bash
g++ -std=c++20 -I cpu-wavelet/include \
  cpu-wavelet/apps/cpu_dwt.cpp \
  cpu-wavelet/src/lifting/scheme_json.cpp \
  cpu-wavelet/src/lifting/plan.cpp \
  cpu-wavelet/src/lifting/executor.cpp \
  cpu-wavelet/src/lifting/cpu_transform.cpp \
  -o cpu_wavelet_cli
```

## Run Accuracy Comparison

PyWavelets is required:

```bash
pip install PyWavelets
```

Then:

```bash
python cpu-wavelet/tools/run_accuracy.py \
  --cpu-bin cpu-wavelet/build/cpu_wavelet_cli
```

Default output:

```text
cpu-wavelet/tests/results/accuracy.csv
```

By default the runner reads lifting schemes from:

```text
cpu-wavelet/lifting_schemes
```

By default the runner skips `coif10` through `coif17`, matching the existing TTNN accuracy harness exclusions for
numerically unstable high-order coiflets. Pass `--include-excluded` to include them.
