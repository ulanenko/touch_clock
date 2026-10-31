# Host Tests

These tests build the pure domain layer (`main/domain/`), the controller core and app actions (`main/app/`)
and the string tables (`main/localization.c`) with the host compiler, without ESP-IDF or LVGL.
`fakes/esp_err.h` stands in for the one ESP-IDF header they use.

## Run

As CI does, under AddressSanitizer and UBSan:

```bash
cmake -S tests/host -B /tmp/touch_clock_host_tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all"
cmake --build /tmp/touch_clock_host_tests
ctest --test-dir /tmp/touch_clock_host_tests --output-on-failure
```

What each test covers: [docs/development.md](../../docs/development.md#host-tests).
