# Railway Status Backend

The status backend is a small Node service in
[../backend/clock-status](../backend/clock-status). It is designed for Railway,
but it can run on any Node 20 host.

## Responsibilities

- Receive clock heartbeats at `POST /api/heartbeat`.
- Return known clocks from `GET /api/clocks`.
- Serve the dashboard from `GET /`.
- Expose `GET /health` for simple uptime checks.

## Railway Configuration

Required variable:

- `CLOCK_STATUS_TOKEN`: shared bearer-style token used by devices and the
  dashboard. Store only in Railway variables or a password manager.

Optional variable:

- `DATA_DIR`: directory for `heartbeats.json`. If omitted, the service stores
  data under `backend/clock-status/data`.

Railway also sets `PORT` automatically.

## Where Notes Live

Railway should be the source of truth for deployment config, variables, logs,
and service health. It should not be the only place for durable runbooks.

Use this split:

- Public runbook: `docs/operations.md`, `docs/ota_releases.md`, and this file.
- Private values: Railway variables and a password manager.
- Private support notes: private docs outside the public repo, or local
  `docs/private_ops_notes.md` for temporary notes. That local file is ignored.

Do not store tokens, customer mapping, or support history in public GitHub.

## Firmware Configuration

Private/test builds can enable telemetry with:

```ini
CONFIG_TOUCH_CLOCK_TELEMETRY_URL="https://<service-domain>/api/heartbeat"
CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN="<clock-status-token>"
CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID="<optional-human-label>"
```

If `CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID` is empty, firmware derives a stable
ID from the station MAC address and shows it in About This Device.

Public OTA builds should leave telemetry URL/token empty unless the project has
a secure provisioning flow.

## Current Limits

- The backend is intentionally small and file-backed.
- For more than a small test fleet, move storage to a Railway Volume or Postgres.
- Remote status is read-only. The current backend does not remotely change time
  settings, force updates, or execute commands on clocks.
- If remote commands are added later, they should be explicit, authenticated,
  auditable, and pulled by the device. Avoid unauthenticated LAN push behavior.
