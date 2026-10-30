# Host Tests

These tests exercise the pure domain layer and the app action layer without ESP-IDF or LVGL.

## Run

```bash
cmake -S tests/host -B /tmp/touch_clock_host_tests
# CI also passes: -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"
cmake --build /tmp/touch_clock_host_tests
ctest --test-dir /tmp/touch_clock_host_tests --output-on-failure
```
