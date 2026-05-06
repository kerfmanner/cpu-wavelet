# CPU Wavelet Lifting

This directory is a standalone CPU implementation area for the lifting wavelet transform.

The design goal is to reuse the same lifting-scheme semantics as `ttnn-wavelet/lifting_schemes`,
while keeping the CPU execution path separate from TT-Metal/N300 code.

## Layout

```text
include/lifting/
  scheme.hpp          Step types, lifting scheme structs, JSON loader declaration.
  shifted_signal.hpp  Reusable signal buffer/view metadata.
  plan.hpp            Plan structs and forward-plan builder declaration.
  executor.hpp        Serial/OpenMP executor abstraction.
  cpu_transform.hpp   One-level forward DWT CPU and batch APIs.

src/lifting/
  scheme_json.cpp     JSON parsing implementation.
  plan.cpp            Shift/offset planning implementation.
  executor.cpp        Non-template executor helpers.
  cpu_transform.cpp   CPU transform implementation.

tests/
  Placeholder for step-by-step CPU/reference tests.
```

## Execution Model

```text
scheme parsing
-> build full execution plan
-> allocate reusable workspace once
-> execute lifting steps sequentially
   -> inside each predict/update step, parallelize over samples only when large enough
   -> scale steps use a separate, larger threshold
-> final crop / output layout
```

Step order is algorithmically sequential. Parallelism is applied inside a step over independent output samples, or at
the batch/channel level for many independent signals.

For OpenMP single-signal execution above the stencil threshold, the transform uses one persistent OpenMP team across
the full lifting chain and synchronizes with barriers between dependent steps. This avoids creating a new OpenMP
parallel region for each predict/update/scale loop.

The initial symmetric split handles boundary samples separately and copies the interior with branch-free strided loops.
Predict/update and scale kernels operate on explicit pointer ranges with restrict-qualified source/base/output pointers
to make aliasing assumptions visible to the compiler.

For many short signals, prefer the batch API:

```text
parallel_for each independent signal/channel
    run the complete DWT with a serial per-signal executor
```

This avoids launching OpenMP work for every small predict/update/scale step. For one large signal, use the normal
single-signal API with OpenMP and tune the stencil and scale thresholds.

Still-open optimization areas:

```text
wavefront / diamond tiling     expose inter-step parallelism for one long signal
workspace arena                reduce allocation/TLB overhead for large batches
mixed precision accumulation   explore float storage with wider SIMD-friendly accumulation choices
```
