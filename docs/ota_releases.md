# OTA Releases

The clock uses ESP-IDF OTA slots and a JSON manifest. Devices check the manifest
over Wi-Fi, compare the manifest version with `TOUCH_CLOCK_FIRMWARE_VERSION`,
download the binary, verify SHA-256, write the inactive OTA slot, and reboot.

## Manifest Format

```json
{
  "version": "1.2",
  "url": "https://example.com/touch_clock-1.2.bin",
  "sha256": "64-character lowercase sha256 hex"
}
```

The current test manifest is [../ota/test/manifest.json](../ota/test/manifest.json).

## Public Release Checklist

1. Update [../main/app_version.h](../main/app_version.h).
2. Confirm the local `sdkconfig` is not staged and is not committed.
3. Build from a public-safe config. Public binaries must not contain telemetry
   tokens, private API keys, Wi-Fi credentials, or customer identifiers.
4. Run a secret scan against source, docs, manifests, and the new binary.
5. Copy the built firmware to `ota/test/touch_clock-X.Y.bin` or the chosen
   release path.
6. Generate SHA-256 and update the manifest.
7. Push the manifest and binary to the public host.
8. Verify the hosted manifest and hosted binary hash before installing.

## Commands

```sh
source ~/esp/v5.5.3/esp-idf/export.sh
idf.py build
cp build/touch_clock.bin ota/test/touch_clock-X.Y.bin
shasum -a 256 ota/test/touch_clock-X.Y.bin
```

Verify the hosted binary after pushing:

```sh
curl -L "https://raw.githubusercontent.com/<owner>/<repo>/<branch>/ota/test/touch_clock-X.Y.bin" \
  | shasum -a 256
```

Check that known private values are absent:

```sh
strings ota/test/touch_clock-X.Y.bin | rg "CLOCK_STATUS_TOKEN|CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN|your-real-token"
```

The `strings` scan is not a cryptographic guarantee, but it catches the common
mistake of embedding plain-text tokens in a public OTA build.

## Versioning

Use simple monotonic versions such as `1.0`, `1.1`, and `1.2` until a formal
release process is needed. The firmware currently treats a higher dotted number
as newer and ignores same-version manifests.

## Rollback

Rollback support is enabled in `sdkconfig.defaults` through
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. A failed app can roll back to the
previous valid OTA slot when ESP-IDF marks the new image invalid.

## Telemetry Caveat

Do not put telemetry tokens into public OTA binaries. For production, use a
separate provisioning path that stores per-device telemetry configuration in
NVS or performs authenticated enrollment with the backend.
