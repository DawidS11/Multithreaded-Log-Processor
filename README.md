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
./build/bin/logproc access.log
```

### Notes

- Build with the same compiler for the normal and TSan presets. Set `CXX` before configuring if you use Homebrew LLVM.
- If you change the file layout, rerun `cmake --preset default` so the test list is regenerated.
- `build/` and `build-tsan/` are generated and ignored by git.