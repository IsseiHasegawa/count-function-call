#include "native_call_counter.h"

#ifdef NCC_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *file;
    char *function;
    int line;
    unsigned long long count;
} NccEntry;

