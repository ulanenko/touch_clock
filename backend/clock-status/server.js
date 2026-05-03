import { createServer } from "node:http";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import crypto from "node:crypto";

const rootDir = dirname(fileURLToPath(import.meta.url));
const dataDir = process.env.DATA_DIR || join(rootDir, "data");
const dataFile = join(dataDir, "heartbeats.json");
const publicDir = join(rootDir, "public");
const port = Number(process.env.PORT || 3000);
const statusToken = process.env.CLOCK_STATUS_TOKEN || "";

let cache = new Map();
let loaded = false;

function sendJson(res, status, payload) {
  const body = JSON.stringify(payload, null, 2);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store",
  });
  res.end(body);
}

function unauthorized(res) {
  sendJson(res, 401, { error: "unauthorized" });
}

function tokenFrom(req, url) {
  const header = req.headers["x-clock-token"];
  if (typeof header === "string" && header.length > 0) {
    return header;
  }
  return url.searchParams.get("token") || "";
}

function authorized(req, url) {
  if (!statusToken) {
    return true;
  }
  const token = tokenFrom(req, url);
  if (token.length !== statusToken.length) {
    return false;
  }
  return crypto.timingSafeEqual(Buffer.from(token), Buffer.from(statusToken));
}

async function loadStore() {
  if (loaded) {
    return;
  }
  loaded = true;
  try {
    const raw = await readFile(dataFile, "utf8");
    const rows = JSON.parse(raw);
    cache = new Map(Object.entries(rows));
  } catch (err) {
    if (err.code !== "ENOENT") {
      throw err;
    }
  }
}

async function saveStore() {
  await mkdir(dataDir, { recursive: true });
  await writeFile(dataFile, JSON.stringify(Object.fromEntries(cache), null, 2));
}

async function readBody(req, limitBytes = 8192) {
  const chunks = [];
  let total = 0;
  for await (const chunk of req) {
    total += chunk.length;
    if (total > limitBytes) {
      throw new Error("body_too_large");
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks).toString("utf8");
}

function cleanText(value, maxLen) {
  if (typeof value !== "string") {
    return "";
  }
  return value.replace(/[^\x20-\x7e]/g, "").slice(0, maxLen);
}

function normalizeHeartbeat(input, req) {
  const now = new Date().toISOString();
  const deviceId = cleanText(input.device_id, 64);
  if (!deviceId) {
    throw new Error("device_id_required");
  }

  return {
    device_id: deviceId,
    label: cleanText(input.label, 64) || deviceId,
    firmware: cleanText(input.firmware, 24),
    ota_partition: cleanText(input.ota_partition, 16),
    ip: cleanText(input.ip, 45),
    wifi_rssi: Number.isFinite(input.wifi_rssi) ? input.wifi_rssi : null,
    time_synced: Boolean(input.time_synced),
    uptime_sec: Number.isFinite(input.uptime_sec) ? Math.max(0, Math.floor(input.uptime_sec)) : null,
    free_heap: Number.isFinite(input.free_heap) ? Math.max(0, Math.floor(input.free_heap)) : null,
    alarm_ringing: Boolean(input.alarm_ringing),
    next_alarm_epoch: Number.isFinite(input.next_alarm_epoch) ? Math.floor(input.next_alarm_epoch) : 0,
    timezone_id: Number.isFinite(input.timezone_id) ? Math.floor(input.timezone_id) : null,
    ota_status: cleanText(input.ota_status, 96),
    source_ip: req.headers["x-forwarded-for"]?.split(",")[0]?.trim() || req.socket.remoteAddress || "",
    updated_at: now,
  };
}

async function handleHeartbeat(req, res, url) {
  if (!authorized(req, url)) {
    unauthorized(res);
    return;
  }
  let body;
  try {
    body = JSON.parse(await readBody(req));
    const row = normalizeHeartbeat(body, req);
    await loadStore();
    cache.set(row.device_id, row);
    await saveStore();
    sendJson(res, 200, { ok: true, server_time: new Date().toISOString() });
  } catch (err) {
    sendJson(res, err.message === "body_too_large" ? 413 : 400, { error: err.message || "bad_request" });
  }
}

async function handleClocks(req, res, url) {
  if (!authorized(req, url)) {
    unauthorized(res);
    return;
  }
  await loadStore();
  const now = Date.now();
  const clocks = [...cache.values()]
    .map((clock) => ({
      ...clock,
      online: now - Date.parse(clock.updated_at) < 15 * 60 * 1000,
      age_sec: Math.max(0, Math.floor((now - Date.parse(clock.updated_at)) / 1000)),
    }))
    .sort((a, b) => a.label.localeCompare(b.label));
  sendJson(res, 200, { clocks });
}

async function serveStatic(res, path) {
  const file = path === "/" ? "index.html" : path.slice(1);
  try {
    const body = await readFile(join(publicDir, file));
    res.writeHead(200, {
      "content-type": file.endsWith(".css") ? "text/css; charset=utf-8" : "text/html; charset=utf-8",
      "cache-control": "no-store",
    });
    res.end(body);
  } catch {
    sendJson(res, 404, { error: "not_found" });
  }
}

const server = createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);

  try {
    if (req.method === "GET" && url.pathname === "/health") {
      sendJson(res, 200, { ok: true });
    } else if (req.method === "GET" && url.pathname === "/api/clocks") {
      await handleClocks(req, res, url);
    } else if (req.method === "POST" && url.pathname === "/api/heartbeat") {
      await handleHeartbeat(req, res, url);
    } else if (req.method === "GET") {
      await serveStatic(res, url.pathname);
    } else {
      sendJson(res, 405, { error: "method_not_allowed" });
    }
  } catch (err) {
    console.error(err);
    sendJson(res, 500, { error: "server_error" });
  }
});

server.listen(port, () => {
  console.log(`touch-clock-status listening on ${port}`);
});
