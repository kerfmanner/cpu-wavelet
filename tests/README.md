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
cpu-wavelet/scripts/build.sh --release --openmp -j 8 --target cpu_wavelet_batch_bench
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

Ubuntu dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build python3 python3-pip
python3 -m pip install numpy PyWavelets matplotlib
```

For maximum CPU performance on Ubuntu, build an optimized OpenMP binary:

```bash
cpu-wavelet/scripts/build.sh --release --openmp --all -j "$(nproc)" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native"
```

If you only need the single-signal benchmark target:

```bash
cpu-wavelet/scripts/build.sh --release --openmp -j "$(nproc)" --target cpu_wavelet_bench \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native"
```

To inspect SIMD/vectorization decisions with GCC, add vectorization diagnostics:

```bash
cpu-wavelet/scripts/build.sh --release --openmp --vec-report -j "$(nproc)" --target cpu_wavelet_bench \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native"
```

Run the 1D vs 1D benchmark from 1M to 100M samples with 100k step:

```bash
python cpu-wavelet/tools/run_performance.py \
  --cpu-bin cpu-wavelet/build/Release-openmp/cpu_wavelet_bench \
  --scheme cpu-wavelet/lifting_schemes/bior1.3.json \
  --kind ramp \
  --start 1000000 \
  --end 100000000 \
  --step 100000 \
  --threads 1 2 4 8 \
  --parallel-threshold 32768 \
  --scale-threshold 131072 \
  --repeats 5 \
  --warmups 1
```

The benchmark binary generates the signal in memory before timing. The timed section measures only the transform call,
not signal reading, CSV parsing, or scheme loading.

When OpenMP is enabled and the signal reaches the stencil threshold, single-signal execution uses one persistent OpenMP
team across the lifting chain with barriers between dependent steps. This removes the fork/join overhead that would
otherwise happen at every lifting step.

Default output:

```text
cpu-wavelet/tests/results/performance.csv
```

Generate plots:

```bash
python cpu-wavelet/tools/plot_performance.py \
  --input cpu-wavelet/tests/results/performance.csv \
  --output-dir cpu-wavelet/tests/results/performance_plots \
  --format svg \
  --xscale log \
  --time-yscale log \
  --ratio-yscale log
```

Default plot outputs are SVG:

```text
cpu-wavelet/tests/results/performance_plots/total_time.svg
cpu-wavelet/tests/results/performance_plots/speedup.svg
cpu-wavelet/tests/results/performance_plots/efficiency.svg
```

Use `--format png` only if PNG output is explicitly needed.

This range creates 991 signal sizes per tested CPU thread count, plus 991 PyWavelets rows. With `--threads 1 2 4 8`,
that is 4,955 CSV rows. The 100M-sample CPU signal is about 400 MB for `float` before workspace buffers; PyWavelets 1D
also allocates coefficient arrays. Make sure the machine has enough RAM.

You can test different executor thread counts without recompilation when the binary is built with `--openmp`:

```bash
python cpu-wavelet/tools/run_performance.py --threads 1 2 4 8 16
```

`--parallel-threshold` controls predict/update and split loops. `--scale-threshold` controls scale-only loops, which are
usually worth parallelizing only at larger sizes.

## CPU 1D vs PyWavelets 2D

This benchmark compares:

```text
cpu-wavelet: one 1D signal with N samples
PyWavelets: one 2D signal with N total elements
```

The default 2D shape uses `--rows 100`, so sizes from 1M to 100M with step 100k become:

```text
1000000 -> 100 x 10000
1100000 -> 100 x 11000
...
100000000 -> 100 x 1000000
```

Run the benchmark:

```bash
python cpu-wavelet/tools/run_pywt2d_vs_cpu1d.py \
  --cpu-bin cpu-wavelet/build/Release-openmp/cpu_wavelet_bench \
  --scheme cpu-wavelet/lifting_schemes/bior1.3.json \
  --wavelet bior1.3 \
  --kind ramp \
  --start 1000000 \
  --end 100000000 \
  --step 100000 \
  --rows 100 \
  --threads 1 2 4 8 \
  --parallel-threshold 32768 \
  --scale-threshold 131072 \
  --repeats 5 \
  --warmups 1
```

Default output:

```text
cpu-wavelet/tests/results/pywt2d_cpu1d_performance.csv
```

Generate SVG plots for this benchmark:

```bash
python cpu-wavelet/tools/plot_performance.py \
  --input cpu-wavelet/tests/results/pywt2d_cpu1d_performance.csv \
  --output-dir cpu-wavelet/tests/results/pywt2d_cpu1d_plots \
  --format svg \
  --xscale log \
  --time-yscale log \
  --ratio-yscale log
```

Default plot outputs:

```text
cpu-wavelet/tests/results/pywt2d_cpu1d_plots/total_time.svg
cpu-wavelet/tests/results/pywt2d_cpu1d_plots/speedup.svg
cpu-wavelet/tests/results/pywt2d_cpu1d_plots/efficiency.svg
```

For PyWavelets 2D at 100M total elements, memory use can be several GB because the input and four coefficient arrays
exist during timing. If the run gets killed by the OS, reduce `--end`, use fewer repeats, or test only CPU first with
`--skip-pywt`.

## Batch Performance

For many short independent signals/channels, benchmark outer batch parallelism with serial per-signal transforms:

```bash
cpu-wavelet/scripts/build.sh --release --openmp -j "$(nproc)" --target cpu_wavelet_batch_bench \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -march=native -mtune=native"

python cpu-wavelet/tools/run_batch_performance.py \
  --batch-bin cpu-wavelet/build/Release-openmp/cpu_wavelet_batch_bench \
  --scheme cpu-wavelet/lifting_schemes/bior1.3.json \
  --kind ramp \
  --start 1024 \
  --end 65536 \
  --step 1024 \
  --batch-sizes 8 16 32 64 \
  --threads 1 2 4 8 \
  --inner-policy serial \
  --repeats 5 \
  --warmups 1
```

Default output:

```text
cpu-wavelet/tests/results/batch_performance.csv
```

## Threshold Sweep

Do not guess the best stencil/scale thresholds. Sweep them by size, wavelet, dtype, and thread count:

```bash
python cpu-wavelet/tools/sweep_thresholds.py \
  --cpu-bin cpu-wavelet/build/Release-openmp/cpu_wavelet_bench \
  --schemes cpu-wavelet/lifting_schemes/bior1.3.json cpu-wavelet/lifting_schemes/db4.json \
  --kinds ramp normal \
  --sizes 100000 250000 500000 1000000 \
  --dtypes float double \
  --threads 1 2 4 8 \
  --stencil-thresholds 8192 32768 131072 524288 \
  --scale-thresholds 32768 131072 524288 1000000000
```

Default output:

```text
cpu-wavelet/tests/results/threshold_sweep.csv
```

Persistent OpenMP regions are implemented for the single-signal OpenMP path. Wavefront/diamond tiling is still a
separate future optimization for exposing inter-step parallelism within one long signal.
