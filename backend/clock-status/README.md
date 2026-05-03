# Touch Clock Status Backend

Small HTTPS-ready heartbeat API and dashboard for multiple Touch Clock devices.

This is intentionally dependency-free so it can run locally, on Railway, or on
any Node host. It stores the latest heartbeat per clock in `data/heartbeats.json`;
for a larger fleet, move this to a persistent Railway Volume or Postgres.

Do not commit real tokens, device/customer mapping, or production heartbeat data
from this service. Operational guidance lives in
[../../docs/railway_status.md](../../docs/railway_status.md).

## API

- `POST /api/heartbeat` stores one device heartbeat.
- `GET /api/clocks` returns the latest known state for every device.
- `GET /` serves the dashboard.
- `GET /health` returns a health check.

Set `CLOCK_STATUS_TOKEN` in production. Devices send it as `X-Clock-Token`; the dashboard accepts the same token in the input field.

## Local Run

```sh
npm start
```

Example heartbeat:

```sh
curl -X POST http://localhost:3000/api/heartbeat \
  -H 'content-type: application/json' \
  -H 'x-clock-token: dev-token' \
  -d '{"device_id":"kitchen","firmware":"1.1","ota_partition":"ota_1","wifi_rssi":-55,"time_synced":true}'
```

## Railway

Deploy from this directory after logging in:

```sh
railway up --detach -m "deploy clock status backend"
railway variable set CLOCK_STATUS_TOKEN="$(openssl rand -hex 24)"
```

Keep the token in Railway variables and a password manager. Do not place the
real value in `README.md`, `sdkconfig.defaults`, or committed OTA artifacts.

## Firmware Config

Set these ESP-IDF config values before building the firmware that should report
status:

```ini
CONFIG_TOUCH_CLOCK_TELEMETRY_URL="https://your-service.up.railway.app/api/heartbeat"
CONFIG_TOUCH_CLOCK_TELEMETRY_TOKEN="<clock-status-token>"
CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID="kitchen"
```

If `CONFIG_TOUCH_CLOCK_TELEMETRY_DEVICE_ID` is empty, the firmware uses the
station MAC address as a stable ID.

Public OTA binaries should be built without telemetry tokens. Use private
builds or a future provisioning flow for devices that should report status.
