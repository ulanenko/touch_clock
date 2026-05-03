# Operations

This is the public-safe operating guide for releases, OTA, and remote status.
It intentionally avoids real tokens, customer data, Wi-Fi credentials, and
private support notes.

## What Belongs Where

Public GitHub repository:

- Firmware source, backend source, partition table, public-safe runbooks.
- OTA manifest and test firmware binaries that are safe for public download.
- Placeholder examples for URLs, tokens, and device labels.

Private storage:

- Railway tokens and service variables.
- Customer/device mapping notes.
- Wi-Fi credentials and support context.
- Local `sdkconfig` values that contain telemetry URLs or tokens.

Use Railway variables for runtime secrets such as `CLOCK_STATUS_TOKEN`. Use a
password manager or a private internal note for long-lived operational notes.
If a local repo note is useful, create `docs/private_ops_notes.md`; it is
ignored by git. Do not commit that file.

## Public-Safe Rules

- Never commit `sdkconfig`; it may contain local telemetry URLs or tokens.
- Never commit real `CLOCK_STATUS_TOKEN` values.
- Never publish an OTA binary that contains a private telemetry token.
- Treat public OTA binaries as customer-downloadable artifacts.
- Keep exact customer/device labels out of public docs unless they are intended
  to be public.
- Before pushing release artifacts, scan for known private values with `rg`.

Example scan:

```sh
rg -n "CLOCK_STATUS_TOKEN|CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN|your-real-token" \
  README.md docs backend main ota sdkconfig.defaults
```

## Current Firmware Release Model

The project uses an OTA-ready partition table:

- `ota_0`: 8 MB app slot
- `ota_1`: 8 MB app slot
- `otadata`: boot slot metadata
- `nvs`: persisted settings
- `storage`: SPIFFS data

See [../partitions.csv](../partitions.csv).

The default OTA manifest URL is configured in
[../sdkconfig.defaults](../sdkconfig.defaults). Local builds may override this
through the untracked `sdkconfig`.

## Current Remote Status Model

The remote status backend lives in
[../backend/clock-status](../backend/clock-status). It accepts heartbeats from
clocks and serves a small dashboard.

Important current limitation:

- Public OTA builds should not embed telemetry tokens.
- A device that installs a public token-free OTA build will stop reporting
  status until telemetry is provisioned again.
- The durable fix is secure provisioning, for example a one-time setup flow
  that stores telemetry config in NVS, or a device-owner backend enrollment
  flow. Reflashing a private local build is acceptable for testing, but it is
  not a scalable customer workflow.

## Operational Docs

- OTA release procedure: [ota_releases.md](ota_releases.md)
- Railway/status backend: [railway_status.md](railway_status.md)
- Backend service README: [../backend/clock-status/README.md](../backend/clock-status/README.md)
