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

Result classes match `wav/usage/test.py` and are based on the worst MSE across approximation/detail outputs:

```text
green  if max(mse_approx, mse_detail) < 5e-6
yellow if max(mse_approx, mse_detail) < 1e-2
red    otherwise
```

## Summarize Accuracy

After `accuracy.csv` exists, aggregate per-wavelet averages into a Markdown table:

```bash
python cpu-wavelet/tools/summarize_accuracy.py
```

Default output:

```text
cpu-wavelet/tests/results/accuracy_summary.md
```

## Performance Benchmark

For maximum CPU performance on Ubuntu, build an optimized OpenMP binary:

```bash
cpu-wavelet/scripts/build.sh --release --openmp -j "$(nproc)" --target cpu_wavelet_bench
```

For GCC or Clang-specific tuning, pass extra CMake flags:

```bash
cpu-wavelet/scripts/build.sh --release --openmp -j "$(nproc)" --target cpu_wavelet_bench \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native"
```

Run the benchmark from 100k to 1M samples with 10k step:

```bash
python cpu-wavelet/tools/run_performance.py \
  --cpu-bin cpu-wavelet/build/Release-openmp/cpu_wavelet_bench \
  --scheme cpu-wavelet/lifting_schemes/bior1.3.json \
  --kind ramp \
  --start 100000 \
  --end 1000000 \
  --step 10000 \
  --threads 1 2 4 8 \
  --repeats 5 \
  --warmups 1
```

The benchmark binary generates the signal in memory before timing. The timed section measures only the transform call,
not signal reading, CSV parsing, or scheme loading.

Default output:

```text
cpu-wavelet/tests/results/performance.csv
```

Generate plots:

```bash
python cpu-wavelet/tools/plot_performance.py
```

Default plot outputs:

```text
cpu-wavelet/tests/results/performance_plots/total_time.png
cpu-wavelet/tests/results/performance_plots/speedup.png
cpu-wavelet/tests/results/performance_plots/efficiency.png
```

You can test different executor thread counts without recompilation when the binary is built with `--openmp`:

```bash
python cpu-wavelet/tools/run_performance.py --threads 1 2 4 8 16
```
