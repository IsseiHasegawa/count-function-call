#include "native_call_counter.h"

#ifdef NCC_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * One entry in the call counter.
 *
 * Example:
 * file     = "src/ujson/python/objToJSON.c"
 * function = "Dict_iterNext"
 * line     = 123
 * count    = 154
 */
typedef struct {
    char *file;
    char *function;
    int line;
    unsigned long long count;
} NccEntry;

/*
 * Dynamic array that stores all recorded function calls.
 */
static NccEntry *entries = NULL;
static size_t entry_count = 0;
static size_t entry_capacity = 0;

/*
 * Whether the call counter has already been initialized.
 */
static int initialized = 0;

/*
 * Copy a string using malloc.
 *
 * strdup is not part of the C standard, so we implement our own version.
 */
static char *ncc_strdup(const char *s) {
    if (s == NULL) {
        s = "";
    }

    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, s, len + 1);
    return copy;
}

/*
 * Write one CSV field with quotes.
 *
 * If the field contains a double quote, escape it by writing two quotes.
 */
static void ncc_write_csv_field(FILE *fp, const char *s) {
    fputc('"', fp);

    if (s != NULL) {
        for (const char *p = s; *p != '\0'; p++) {
            if (*p == '"') {
                fputc('"', fp);
            }

            fputc(*p, fp);
        }
    }

    fputc('"', fp);
}

/*
 * Dump the collected call counts to a CSV file.
 */
void ncc_dump(void) {
    const char *output_path = getenv("NCC_OUTPUT");

    if (output_path == NULL || output_path[0] == '\0') {
        output_path = "native_call_counts.csv";
    }

    FILE *fp = fopen(output_path, "w");

    if (fp == NULL) {
        return;
    }

    fprintf(fp, "file,function,line,count\n");

    for (size_t i = 0; i < entry_count; i++) {
        ncc_write_csv_field(fp, entries[i].file);
        fprintf(fp, ",");

        ncc_write_csv_field(fp, entries[i].function);
        fprintf(fp, ",%d,%llu\n", entries[i].line, entries[i].count);
    }

    fclose(fp);
}

/*
 * Initialize the call counter.
 *
 * atexit registers ncc_dump so that the CSV file is written
 * when the process exits.
 */
void ncc_init(void) {
    if (initialized) {
        return;
    }

    initialized = 1;
    atexit(ncc_dump);
}

/*
 * Record one function call.
 *
 * If the function already exists in entries, increment its count.
 * Otherwise, add a new entry with count = 1.
 */
void ncc_record_call(const char *file, const char *function, int line) {
    if (!initialized) {
        ncc_init();
    }

    for (size_t i = 0; i < entry_count; i++) {
        if (
            entries[i].line == line &&
            strcmp(entries[i].file, file) == 0 &&
            strcmp(entries[i].function, function) == 0
        ) {
            entries[i].count++;
            return;
        }
    }

    if (entry_count == entry_capacity) {
        size_t new_capacity = entry_capacity == 0 ? 64 : entry_capacity * 2;

        NccEntry *new_entries = (NccEntry *)realloc(
            entries,
            new_capacity * sizeof(NccEntry)
        );

        if (new_entries == NULL) {
            return;
        }

        entries = new_entries;
        entry_capacity = new_capacity;
    }

    entries[entry_count].file = ncc_strdup(file);
    entries[entry_count].function = ncc_strdup(function);
    entries[entry_count].line = line;
    entries[entry_count].count = 1;

    entry_count++;
}

#endif