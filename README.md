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
  cpu_transform.hpp   One-level forward DWT CPU API.

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
   -> inside each step, parallelize over samples only when large enough
-> final crop / output layout
```

Step order is algorithmically sequential. Parallelism is applied inside a step over independent output samples, or at
the batch/channel level for many independent signals.
