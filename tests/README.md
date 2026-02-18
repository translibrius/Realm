# Tests

Realm uses a lightweight in-house C test harness integrated with CMake/CTest.

## Common workflow

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Run app with tests first

```bash
cmake --build --preset debug --target run_realm_checked
```

This runs the unit suite first, then launches `Realm` if tests pass.

## Harness commands

From `build/debug`:

```bash
bin/RealmTests --list
bin/RealmTests --filter str
bin/RealmTests --fail-fast
```

## Adding new tests

1. Add a test file under `tests/cases/`.
2. Add a `register_<suite>_tests(void)` function in that file.
3. Register it in `tests/main.c`.
4. Add the file to `tests/CMakeLists.txt`.
