#pragma once

#include "defines.h"

b8 rl_test_runtime_init(void);
void rl_test_runtime_shutdown(void);

// Suppress/restore stdout+stderr. Use around code that triggers engine log
// noise (e.g. async logger writer) to keep test output clean.
void rl_test_suppress_console(void);
void rl_test_restore_console(void);
