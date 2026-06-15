#ifndef NATIVE_CALL_COUNTER_H
#define NATIVE_CALL_COUNTER_H

/*
 * Make the functions callable from C++ code.
 * Without this, C++ compilers may change function names internally
 * through name mangling.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NCC_ENABLE controls whether call counting is enabled.
 *
 * If NCC_ENABLE is defined at compile time, NCC_RECORD_CALL()
 * records function calls.
 *
 * If NCC_ENABLE is not defined, NCC_RECORD_CALL() does nothing.
 */
#ifdef NCC_ENABLE

void ncc_init(void);
void ncc_record_call(const char *file, const char *function, int line);
void ncc_dump(void);

#define NCC_RECORD_CALL() ncc_record_call(__FILE__, __func__, __LINE__)
#else

static inline void ncc_init(void) {}

static inline void ncc_record_call(const char *file, const char * function, int line) {
    (void)file;
    (void)function;
    (void)line;
}

static inline void ncc_dump(void) {}

#define NCC_RECORD_CALL() ((void)0)

#endif

#ifdef __cplusplus

}
#endif

#endif