#include "util/assert.h"

#include "core/logger.h"

void report_assertion_failure(const char* expression,
                              const char* message,
                              const char* file,
                              i32 line) {
    RL_FATAL("Assertion failure: %s, message: %s, in file: %s, line: %d",
             expression, message, file, line);
}