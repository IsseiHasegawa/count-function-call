# count-function-call

Native Call Counter is a small C/C++ instrumentation library for recording how many times target native functions are called during a test run.

It is designed for projects where the implementation is written in C or C++, but the tests may be written in another language, such as Python with `pytest`.

For example, a Python test may call a Python API, which internally calls C/C++ functions. By inserting `NCC_RECORD_CALL();` into the target native functions, this tool records how often each function is executed.

## Goal

The goal of this project is to answer questions such as:

* Was this C/C++ function executed during the test suite?
* How many times was this function called?
* Which native functions are covered by tests written in another language?

This is useful for analyzing native code coverage and for research on pseudo-tested code.

## Repository Structure

```text
native-call-counter/
├── include/
│   └── native_call_counter.h
├── src/
│   └── native_call_counter.c
└── README.md
```

## How It Works

Target C/C++ functions are manually instrumented with:

```c
NCC_RECORD_CALL();
```

When instrumentation is enabled, this macro expands to:

```c
ncc_record_call(__FILE__, __func__, __LINE__);
```

This records:

* the source file name
* the function name
* the line number where the macro is inserted
* the number of times the function was called

At the end of the process, the collected results are written to a CSV file.

## Example

Suppose a target project has the following C function:

```c
static int Dict_iterNext(JSOBJ obj, JSONTypeContext *tc) {
    NCC_RECORD_CALL();

    /* original function body */
}
```

If this function is called during a test run, Native Call Counter records the call.

Example CSV output:

```csv
file,function,line,count
"src/ujson/python/objToJSON.c","Dict_iterNext",123,154
"src/ujson/python/objToJSON.c","Dict_iterEnd",145,38
```

## Building With a Target Project

This library is intended to be compiled together with the target project.

The target project must include:

```c
#include "native_call_counter.h"
```

or force-include the header at compile time.

The implementation file must also be added to the target build:

```text
native-call-counter/src/native_call_counter.c
```

Instrumentation is enabled by defining:

```text
NCC_ENABLE
```

For example, a compiler command may include:

```bash
-DNCC_ENABLE
```

## Output File

By default, the output file is:

```text
native_call_counts.csv
```

You can change the output path using the `NCC_OUTPUT` environment variable:

```bash
NCC_OUTPUT=my_call_counts.csv ./target_program
```

For Python-based tests, for example:

```bash
NCC_OUTPUT=ujson_call_counts.csv python -m pytest
```

## Integration Example With a Python C Extension

For a Python package with C/C++ extension code, the flow is:

```text
1. Insert NCC_RECORD_CALL(); into target C/C++ functions.
2. Compile the extension with NCC_ENABLE enabled.
3. Run the Python test suite, such as pytest.
4. Inspect the generated CSV file.
```

Example:

```bash
NCC_ENABLE=1 \
NCC_REPO=../native-call-counter \
python setup.py build_ext --inplace --force
```

Then run:

```bash
NCC_OUTPUT=native_call_counts.csv python -m pytest
```

## Interpretation

If a function appears in the CSV file, it was executed during the test run.

If a function does not appear in the CSV file, it was not observed during the test run.

This can be used as a first step before mutation-based analysis:

```text
Function not called
→ uncovered

Function called + mutation causes test failure
→ tested / killed

Function called + mutation does not cause test failure
→ pseudo-tested candidate
```

## Limitations

This tool currently provides a simple function-call counter.

Current limitations:

* It requires manual insertion of `NCC_RECORD_CALL();`.
* It does not automatically instrument all functions.
* The current implementation is intended for simple single-process or basic test runs.
* Multi-threaded programs may require synchronization support.
* Parallel test execution may require separate output files per process.

## License

TBD
