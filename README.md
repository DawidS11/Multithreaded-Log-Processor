# Multithreaded-Log-Processor
The project reads log files, parses records, processes them with a worker pool, aggregates statistics, and outputs a report.

The core flow is a reader thread that pulls big blocks from the file and splits them into lines, grouped into batches. Those batches go into a bounded queue. A pool of worker threads takes batches, parses them, and updates its own local statistics with no shared locks. When the file ends, the workers’ results are merged into one report.


## Testing

The project uses GoogleTest and CTest. Tests are discovered automatically from the build, so you only need to build and run them.

### Prerequisites

- CMake 3.25 or newer
- A C++23 compiler

### Run the normal test suite

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --timeout 120
```

Binaries are written to `build/bin/`. To run one test suite directly with its full output:

```bash
./build/bin/queue_tests
./build/bin/pipeline_tests
```

| Binary | What it checks |
|---|---|
| `loggen_tests` | Log generator output format and determinism |
| `logparser_tests` | Parsing of valid and malformed lines, and generator/parser round trip |
| `queue_tests` | `BoundedQueue` ordering, blocking, close behavior, and a multi-producer, multi-consumer stress test |
| `pipeline_tests` | Full analysis of 100,000 lines with several worker counts, checking identical reports |

The `--timeout` flag matters. A deadlock in the queue or shutdown code shows up as a test that never returns, so the timeout turns a hang into a failure.

### Run the tests under ThreadSanitizer

ThreadSanitizer (TSan) reports data races in multithreaded code. It has its own build, so the normal build is not affected:

```bash
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --timeout 300
```

TSan binaries are written to `build-tsan/bin/`. For the full report from the queue tests:

```bash
TSAN_OPTIONS="halt_on_error=1" ./build-tsan/bin/queue_tests
```

A clean run prints no `WARNING: ThreadSanitizer` messages. TSan runs several times slower than a normal build, so allow more time.

### Run the program

```bash
cmake --build --preset default
./build/bin/loggen 100000 > access.log
./build/bin/logproc access.log --workers 2 --batch 100
```

The processor can be run in either threaded or single-threaded mode from the terminal without rebuilding:

```bash
LOGPROC_MODE=single ./build/bin/logproc access.log
./build/bin/logproc access.log
```

- `LOGPROC_MODE=single` selects `analyzeWithoutThreads`
- the default path uses the threaded `analyze` implementation

### Notes

- Build with the same compiler for the normal and TSan presets. Set `CXX` before configuring if you use Homebrew LLVM.
- If you change the file layout, rerun `cmake --preset default` so the test list is regenerated.
- `build/` and `build-tsan/` are generated and ignored by git.

## Results
10'000'000 logs, average across 5 runs, input size 811 MiB:

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

The best average result is the 8-worker configuration with a batch size of 10,000, at 0.732 s and about 1,108 MiB/s, which is about 3.74x faster than the single-threaded baseline.