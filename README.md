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
├── ncc/                    # auto-instrumentation tool (Python)
├── pyproject.toml
└── README.md
```

## How it works

```text
1. Instrument   ncc-instrument adds NCC_RECORD_CALL() to each function
2. Build        NCC_ENABLE=1 links native_call_counter.c into the extension
3. Test         pytest runs; each call is counted in memory
4. CSV          written at process exit (atexit)
```

With `NCC_ENABLE` defined at compile time, `NCC_RECORD_CALL()` records `file`, `function`, `line`, and `count`. Without it, the macro is a no-op.

---

## Quick start (ultrajson example)

Assumes this layout:

```text
PsedoClang/
├── .venv/                  # tool venv (ncc-instrument)
├── count-function-call/
└── ultrajson/
    └── .venv/              # target venv (build + pytest)
```

### One-time setup

```bash
# Tool venv (at repo root)
cd PsedoClang
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -e ./count-function-call

# Target venv
cd ultrajson
python3 -m venv .venv
source .venv/bin/activate
python -m pip install setuptools setuptools-scm pytest
```

### Every run 

```bash
# Step 1 — instrument (from PsedoClang root, tool venv active)
cd PsedoClang
source .venv/bin/activate
ncc-instrument --file ultrajson/src/ujson/python/objToJSON.c

# Step 2 — build with NCC enabled
cd ultrajson
source .venv/bin/activate
NCC_ENABLE=1 NCC_REPO=../count-function-call pip install -e .

# Step 3 — run tests, write CSV
NCC_OUTPUT=native_call_counts.csv python -m pytest -q

# Step 4 — read results
cat native_call_counts.csv
```

---

## Step 1: Instrument sources

### Auto-instrument (`ncc-instrument`)

Install once (from a venv — do not use system `pip` on macOS/Homebrew Python):

```bash
cd PsedoClang
source .venv/bin/activate
python -m pip install -e ./count-function-call
```

Preview (does not modify files):

```bash
ncc-instrument --dry-run --file ultrajson/src/ujson/python/objToJSON.c
```

Apply:

```bash
ncc-instrument --file ultrajson/src/ujson/python/objToJSON.c
```

### Specifying files

`--file` takes the **path to a C/C++ source file** (relative to your current directory, or absolute).

```bash
# Single file (from PsedoClang root)
ncc-instrument --file ultrajson/src/ujson/python/objToJSON.c

# Multiple files
ncc-instrument \
  --file ultrajson/src/ujson/python/objToJSON.c \
  --file ultrajson/src/ujson/python/JSONtoObj.c \
  --file ultrajson/src/ujson/lib/ultrajsonenc.c

# From inside count-function-call/
ncc-instrument --file ../ultrajson/src/ujson/python/objToJSON.c
```

The tool:

- inserts `#include "native_call_counter.h"` after the last `#include` (if missing)
- inserts `NCC_RECORD_CALL();` at the start of every function body in that file
- skips functions that already contain `NCC_RECORD_CALL()` (safe to re-run)
- skips `ncc_record_call`, `ncc_init`, and `ncc_dump` by default

`functions_instrumented=0` means the file was already instrumented.

### Manual instrumentation

```c
#include "native_call_counter.h"

static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc) {
    NCC_RECORD_CALL();
    /* ... */
}
```

---

## Step 2: Build with NCC enabled

After instrumentation, the target **must** be built with `NCC_ENABLE=1` so the header is found and counting is active.

| Requirement | Value |
|-------------|-------|
| Header | `#include "native_call_counter.h"` |
| Source | `count-function-call/src/native_call_counter.c` |
| Compile flag | `-DNCC_ENABLE` |

### ultrajson

```bash
cd ultrajson
source .venv/bin/activate

NCC_ENABLE=1 \
NCC_REPO=../count-function-call \
pip install -e .
```

Or:

```bash
NCC_ENABLE=1 \
NCC_REPO=../count-function-call \
python setup.py build_ext --inplace --force
```

Confirm the build log contains `-DNCC_ENABLE=1`, `-I../count-function-call/include`, and `native_call_counter.c`.

Rebuild after changing instrumentation or C sources.

### Generic C/C++ binary

```bash
clang -DNCC_ENABLE \
  -I../count-function-call/include \
  main.c ../count-function-call/src/native_call_counter.c \
  -o my_program
```

---

## Step 3: Run tests and write CSV

The CSV is written when the **Python process exits** (`atexit`), not at build time.

```bash
cd ultrajson
source .venv/bin/activate

# Full suite
NCC_OUTPUT=native_call_counts.csv python -m pytest -q

# Single test
NCC_OUTPUT=one_test.csv \
python -m pytest tests/test_ujson.py::test_encode_dict_conversion -q
```

---

## Step 4: Read the CSV

```bash
cat native_call_counts.csv
grep objToJSON.c native_call_counts.csv
```

Example:

```csv
file,function,line,count
"./src/ujson/python/objToJSON.c","Dict_iterNext",262,6682
"./src/ujson/python/objToJSON.c","objToJSON",699,10364
```

| Column | Meaning |
|--------|---------|
| `file` | Source file path |
| `function` | C function name |
| `line` | Line of `NCC_RECORD_CALL()` |
| `count` | Times called during the run |

- Row present → function was called during the test run.
- Row absent → function was not called (or not instrumented).

Only **called** functions appear. Instrumenting a file does not list every function — only those executed at runtime.

---

## Environment variables

| Variable | When | Default | Description |
|----------|------|---------|-------------|
| `NCC_ENABLE=1` | Build | off | Enable counting (`NCC_RECORD_CALL` records calls) |
| `NCC_REPO` | Build | `../count-function-call` | Path to this repo (for `ultrajson/setup.py`) |
| `NCC_OUTPUT` | Test run | `native_call_counts.csv` | CSV output path (relative to cwd) |

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `ncc-instrument: command not found` | `source PsedoClang/.venv/bin/activate` (install from repo root, not inside `count-function-call/`) |
| `externally-managed-environment` | Use a venv; run `python -m pip`, not bare `pip` on Homebrew Python |
| `native_call_counter.h: file not found` | Build with `NCC_ENABLE=1 NCC_REPO=../count-function-call` |
| Empty CSV | Built without `NCC_ENABLE=1` — rebuild and re-run tests |
| `functions_instrumented=0` | Already instrumented; no action needed |
| pytest passes but CSV unchanged | Re-run tests after rebuild; CSV updates on process exit |

**venv tip:** Run `ncc-instrument` from the **PsedoClang repo root** with the root `.venv` active. If your shell auto-deactivates venv when entering `count-function-call/`, use the full path:

```bash
PsedoClang/.venv/bin/ncc-instrument --file ultrajson/src/ujson/python/objToJSON.c
```

---

## Interpretation (pseudo-testing workflow)

```text
Not in CSV                    → uncovered by tests
In CSV + mutation fails tests → tested
In CSV + mutation passes tests → pseudo-tested candidate
```

## Limitations

- Auto-instrumentation uses Tree-sitter; macro-generated functions may be missed
- Counts are per **process** (whole test suite total), not per individual test
- Single-process / simple test runs; no thread safety
- Parallel pytest (`pytest-xdist`) may overwrite CSV per worker unless each process uses a different `NCC_OUTPUT`
- After instrumentation, a normal build without `NCC_ENABLE` will fail unless you remove the `#include`

## License
