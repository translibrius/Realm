# Testing

## Running tests

```bash
ctest --preset debug

# Or run the binary directly from build/debug/
bin/RealmTests --list
bin/RealmTests --filter <pattern>
bin/RealmTests --fail-fast

# Run tests then launch the app
cmake --build --preset debug --target run_realm_checked
```

## Adding a test

`tests/cases/test_<name>.c` → implement `register_<name>_tests()` → register in `tests/main.c` → add to `tests/CMakeLists.txt`.

## What to test

This is a hobby project — don't aim for corporate-level coverage, but do write tests where they're genuinely useful:

- **Do test**: core subsystems (memory allocator, arena, events, string utils, config parsing), anything with tricky logic that can break silently, pure functions that are easy to test without mocking.
- **Don't bother testing**: rendering output, platform-specific window management, GUI layout visuals, things that require complex mocking for little value.
- When adding or modifying a subsystem with testable logic, check if tests exist and add/update them. If a new file has functions that are easy to unit test, write tests for them.
