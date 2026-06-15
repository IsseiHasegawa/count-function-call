#include "native_call_counter.h"

#ifdef NCC_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int count:
    char file[PATH_MAX];
    char function[FUNC_MAX];
} ncc_call_t;