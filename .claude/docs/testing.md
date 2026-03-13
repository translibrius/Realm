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

## Testing integrity

- **Test behavior, not implementation.** Assert what the function promises, not how it works internally. Never re-implement logic in a test to compare against itself — that's circular verification.
- **Tests must be able to fail.** If a broken implementation still passes the test, the test is worthless. Before writing an assertion, ask: "what bug would this catch?"
- **No trivial assertions.** `!= NULL` or `> 0` alone is not a test. Verify actual values, round-trip data, or observable side effects.
- **No flaky timing.** Don't `sleep()` and hope. If testing async behavior, use deterministic synchronization or skip.
- **Test the public API.** Call the same functions users of the subsystem call, not internal helpers.
