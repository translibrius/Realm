#include "util/assert.h"

#include <stdio.h>

void report_assertion_failure(const char* expression,
                              const char* message,
                              const char* file,
                              i32 line) {
    printf("Assertion failure: %s, message: %s, in file: %s, line: %d\n",
           expression, message, file, line);
}