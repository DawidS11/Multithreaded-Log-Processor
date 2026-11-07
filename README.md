# Multithreaded Log Processor

A C++23 command-line application that generates Apache-style access logs, parses them, and reports traffic and latency statistics. The processor supports both a bounded-queue worker pool and a single-threaded mode for comparison.

## Features

- Parses Apache-style access log records with a UTC timestamp, request, status, response size, optional referrer/user-agent fields, and a trailing latency value in microseconds. The referrer and user-agent fields are either both present or both omitted.
- Counts parsed and malformed lines, including malformed-line totals by reason.
- Reports response bytes, average and maximum latency, counts by status code, and requests grouped by UTC minute.
- Estimates p95 and p99 latency ranges using a fixed-size, log-spaced histogram; percentile values are approximate bucket ranges, not exact samples.
- Counts the top ten normalized endpoints. Query strings are removed and numeric path segments are replaced with `:id` (for example, `/users/42?expand=true` becomes `/users/:id`).
- Processes input in batches through a bounded FIFO queue. Each worker maintains its own statistics, which are merged after input is consumed.

## Build and test

### Requirements

- CMake 3.25 or newer (the project presets use CMake Presets version 6)
- A compiler with C++23 support
- Network access on the first configure, to fetch GoogleTest

Configure, build, and run the complete GoogleTest/CTest suite:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --timeout 120
```

Executables are placed in `build/bin/`. Test suites can also be run directly:

```bash
./build/bin/loggen_tests
./build/bin/logparser_tests
./build/bin/queue_tests
./build/bin/pipeline_tests
./build/bin/cli_tests
```

| Test binary | Coverage |
|---|---|
| `loggen_tests` | Generated line count, determinism, timestamps, format, and field ranges |
| `logparser_tests` | Valid and malformed log records, plus generator/parser compatibility |
| `queue_tests` | Queue ordering, blocking, close behavior, and concurrent stress behavior |
| `pipeline_tests` | Multi-worker aggregation, malformed-line counts, minute and endpoint grouping, and latency histogram reporting |
| `cli_tests` | Valid and invalid processor command-line arguments |

The CTest timeout helps turn a queue deadlock or shutdown hang into a test failure instead of an indefinitely running test.

### ThreadSanitizer

The `tsan` preset builds a separate Debug configuration with ThreadSanitizer enabled:

```bash
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --timeout 300
```

TSan binaries are placed in `build-tsan/bin/`. To run the queue tests directly and stop at the first detected race:

```bash
TSAN_OPTIONS="halt_on_error=1" ./build-tsan/bin/queue_tests
```

A clean run has no `WARNING: ThreadSanitizer` messages. TSan is slower than a normal build. Use the same compiler for the normal and TSan builds; set `CXX` before configuring if using Homebrew LLVM.

## Run the programs

Generate 100,000 log lines and analyze them with four workers and batches of 1,000 lines:

```bash
./build/bin/loggen 100000 > access.log
./build/bin/logproc access.log --workers 4 --batch 1000
```

`loggen` accepts one non-negative line count and writes the generated records to standard output. Its synthetic traffic rate is hardcoded at approximately 5,000 requests per second (300,000 per minute). Each 100-request latency cycle contains 90 fast requests (about 1–10.8 ms) and a 10-request tail spanning 10–100 ms. Latency is written in microseconds.

The processor syntax is:

```text
logproc <logfile-path> [--workers <n>] [--batch <m>]
```

Both options are optional and must follow the path, each requires a positive integer. Defaults are the machine's reported hardware concurrency for `--workers` and `10000` lines for `--batch`. The timing summary is written to standard error, the analysis report is written to standard output.

### Select threaded or single-threaded mode

The default is the threaded worker-pool implementation. Set `LOGPROC_MODE=single` to process lines sequentially without the queue or worker threads; this does not require a separate build or command-line option:

```bash
LOGPROC_MODE=single ./build/bin/logproc access.log
./build/bin/logproc access.log --workers 4 --batch 1000
```

### Report contents

The report includes line/parse totals, response bytes, average and maximum latency, status-code counts, request counts by minute, approximate p95/p99 latency ranges, and the ten most frequent normalized endpoints. Requests-per-minute values are currently printed as Unix-epoch minute keys. When malformed records are present, their counts are listed by parse-error reason.

## Recorded performance results

The following measurements are the average of five runs on a 10,000,000-line input file of 811 MiB. Results depend on the machine, compiler, and input; rerun the benchmark when comparing a different environment or generated dataset.

| Mode | Workers | Batch size | Average time (s) | Throughput (MiB/s) | Speedup vs single-threaded |
|---|---:|---:|---:|---:|---:|
| Single-threaded without batches | — | — | 2.740 | 296.0 | 1.00x |
| Multi-threaded with batches | 1 | 100 | 2.638 | 307.4 | 1.04x |
| Multi-threaded with batches | 2 | 100 | 1.359 | 596.6 | 2.01x |
| Multi-threaded with batches | 4 | 100 | 1.001 | 810.2 | 2.74x |
| Multi-threaded with batches | 8 | 100 | 1.212 | 669.3 | 2.26x |
| Multi-threaded with batches | 1 | 1000 | 2.218 | 365.6 | 1.23x |
| Multi-threaded with batches | 2 | 1000 | 1.266 | 640.5 | 2.16x |
| Multi-threaded with batches | 4 | 1000 | 0.857 | 946.5 | 3.20x |
| Multi-threaded with batches | 8 | 1000 | 0.812 | 998.8 | 3.37x |
| Multi-threaded with batches | 1 | 10000 | 2.181 | 371.7 | 1.26x |
| Multi-threaded with batches | 2 | 10000 | 1.102 | 735.9 | 2.49x |
| Multi-threaded with batches | 4 | 10000 | 0.813 | 997.5 | 3.37x |
| Multi-threaded with batches | 8 | 10000 | 0.732 | 1108.2 | 3.74x |

The best recorded average is the 8-worker configuration with a batch size of 10,000: 0.732 s, about 1,108 MiB/s, or 3.74x the single-threaded baseline.

## Implementation notes

- The caller thread reads the input with `std::getline`, groups lines into batches, and pushes them to a bounded queue with capacity for two batches per worker.
- Worker threads parse their assigned batches and update worker-local statistics; the main thread merges those results after the queue is closed and the workers finish.
- `LOGPROC_MODE=single` uses the line-by-line `analyzeWithoutThreads` path instead.
- `build/` and `build-tsan/` are generated build directories and are ignored by Git.