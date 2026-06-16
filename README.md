# count-function-call

A small C/C++ library that counts how often instrumented native functions are called during a test run.

Use it when the implementation is C/C++ but tests run in another language (e.g. Python `pytest` calling a C extension).

## What it answers

- Was this C function executed during the test suite?
- How many times was it called?
- Which native functions are exercised by higher-level tests?

## Layout

```text
count-function-call/
├── include/native_call_counter.h
├── src/native_call_counter.c
└── README.md
```

## Instrumentation

Add the header and macro to each function you want to track:

```c
#include "native_call_counter.h"

static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc) {
    NCC_RECORD_CALL();
    /* ... */
}
```

With `NCC_ENABLE` defined at compile time, the macro records `file`, `function`, `line`, and `count`. Without it, `NCC_RECORD_CALL()` is a no-op.

## Build integration

Link `native_call_counter.c` into the target binary and define `NCC_ENABLE`.

| Requirement | Value |
|-------------|-------|
| Header | `#include "native_call_counter.h"` |
| Source | `count-function-call/src/native_call_counter.c` |
| Compile flag | `-DNCC_ENABLE` (or `define_macros=[("NCC_ENABLE", "1")]`) |

---

## Example: ultrajson (Python C extension)

Assumes this repo layout:

```text
PesudoScope/
├── count-function-call/
└── ultrajson/
```

### 1. Instrument C sources

In each target `.c` file:

```c
#include "native_call_counter.h"
```

At the start of each function body:

```c
NCC_RECORD_CALL();
```

### 2. Build the extension with NCC enabled

From `PesudoScope/ultrajson` (not `PesudoScope/ultrajson/ultrajson`):

```bash
cd ultrajson
source .venv/bin/activate
```

```bash
NCC_ENABLE=1 \
NCC_REPO=../count-function-call \
python setup.py build_ext --inplace --force
```

| Variable | Purpose |
|----------|---------|
| `NCC_ENABLE=1` | Enables counting (`NCC_RECORD_CALL` records calls) |
| `NCC_REPO` | Path to this repo (default in `ultrajson/setup.py`: `../count-function-call`) |

Confirm the build log contains `-DNCC_ENABLE=1` and compiles `native_call_counter.c`.

Rebuild after changing instrumentation or C sources:

```bash
NCC_ENABLE=1 NCC_REPO=../count-function-call python setup.py build_ext --inplace --force
```

### 3. Run tests and write CSV

The CSV is written when the **Python process exits** (`atexit`), not at build time.

```bash
NCC_OUTPUT=native_call_counts.csv python -m pytest
```

| Variable | Default | Description |
|----------|---------|-------------|
| `NCC_OUTPUT` | `native_call_counts.csv` | Output path (relative to current working directory) |

If you run pytest from `ultrajson/`, the file is:

```text
ultrajson/native_call_counts.csv
```

Run a single test:

```bash
NCC_OUTPUT=one_test.csv \
python -m pytest tests/test_ujson.py::test_encode_list_conversion -q
```

Run the full suite:

```bash
NCC_OUTPUT=ujson_call_counts.csv python -m pytest
```

### 4. Read the CSV

```bash
cat native_call_counts.csv
```

Example:

```csv
file,function,line,count
"./src/ujson/python/objToJSON.c","PyStringToUTF8",112,42
```

| Column | Meaning |
|--------|---------|
| `file` | Source file path |
| `function` | C function name |
| `line` | Line of `NCC_RECORD_CALL()` |
| `count` | Times called during the run |

- Row present → function was called.
- Row absent → function was not called (or not instrumented).

---

## Generic C/C++ binary

```bash
# compile (example)
clang -DNCC_ENABLE \
  -I../count-function-call/include \
  main.c ../count-function-call/src/native_call_counter.c \
  -o my_program

# run
NCC_OUTPUT=my_call_counts.csv ./my_program
```

---

## Interpretation (pseudo-testing workflow)

```text
Not in CSV          → uncovered by tests
In CSV + mutation fails tests → tested
In CSV + mutation passes tests  → pseudo-tested candidate
```

## Limitations

- Manual `NCC_RECORD_CALL()` insertion only
- Single-process / simple test runs; no thread safety
- Parallel pytest (`pytest-xdist`) may overwrite CSV per worker unless each process uses a different `NCC_OUTPUT`

## License

TBD
