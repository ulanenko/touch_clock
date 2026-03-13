#!/usr/bin/env node

import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceRoot = path.resolve(__dirname, '..');

const requestedPath = process.argv[2] ?? '3d_model.scad';
const port = Number(process.argv[3] ?? process.env.PORT ?? 4173);
const scadPath = path.resolve(workspaceRoot, requestedPath);
const scadRoute = `/${path.basename(scadPath)}`;
const hostedPlayground = 'https://ochafik.com/openscad2/';
const hostedPlaygroundPath = '/openscad2';
const proxiedPlaygroundBase = '/openscad2/';

async function getFileInfo() {
  const stats = await fs.stat(scadPath);
  return {
    size: stats.size,
    mtimeMs: Math.trunc(stats.mtimeMs),
    version: `${Math.trunc(stats.mtimeMs)}-${stats.size}`,
  };
}

async function readScadFile() {
  return fs.readFile(scadPath, 'utf8');
}

function sendJson(res, status, payload) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
    'Access-Control-Allow-Origin': '*',
  });
  res.end(JSON.stringify(payload));
}

function sendHtml(res, html) {
  res.writeHead(200, {
    'Content-Type': 'text/html; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  res.end(html);
}

function buildHtml() {
  return `<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>OpenSCAD Playground Preview</title>
    <style>
      :root {
        color-scheme: light;
        font-family: ui-sans-serif, system-ui, sans-serif;
        background: #f3efe5;
        color: #1a1a1a;
      }
      * { box-sizing: border-box; }
      body {
        margin: 0;
        min-height: 100vh;
        display: grid;
        grid-template-rows: auto 1fr;
        background:
          radial-gradient(circle at top left, rgba(227, 184, 115, 0.28), transparent 28rem),
          linear-gradient(180deg, #f7f2e8 0%, #efe8d7 100%);
      }
      header {
        padding: 0.9rem 1rem;
        border-bottom: 1px solid rgba(26, 26, 26, 0.12);
        display: flex;
        gap: 1rem;
        flex-wrap: wrap;
        align-items: center;
        justify-content: space-between;
        background: rgba(255, 252, 245, 0.82);
        backdrop-filter: blur(10px);
      }
      .meta {
        display: flex;
        gap: 1rem;
        flex-wrap: wrap;
        align-items: center;
      }
      .pill {
        padding: 0.4rem 0.65rem;
        border-radius: 999px;
        background: rgba(255, 255, 255, 0.76);
        border: 1px solid rgba(26, 26, 26, 0.1);
        font-size: 0.92rem;
      }
      a {
        color: inherit;
      }
      iframe {
        width: 100%;
        height: 100%;
        border: 0;
        background: white;
      }
      main {
        min-height: 0;
      }
    </style>
  </head>
  <body>
    <header>
      <div class="meta">
        <div class="pill"><strong>SCAD:</strong> <code>${escapeHtml(scadPath)}</code></div>
        <div class="pill"><strong>Status:</strong> <span id="status">Loading…</span></div>
        <div class="pill"><strong>Last change:</strong> <span id="updated-at">-</span></div>
      </div>
      <div class="pill">
        Embedded from <a href="${hostedPlayground}" target="_blank" rel="noreferrer">openscad-playground</a>
      </div>
    </header>
    <main>
      <iframe
        id="preview"
        title="OpenSCAD preview"
        allow="xr-spatial-tracking; fullscreen"
        referrerpolicy="no-referrer"
      ></iframe>
    </main>
    <script>
      const iframe = document.getElementById('preview');
      const statusEl = document.getElementById('status');
      const updatedAtEl = document.getElementById('updated-at');
      let currentVersion = null;

      function buildIframeSrc(version) {
        const fileUrl = new URL('${scadRoute}', window.location.origin);
        fileUrl.searchParams.set('v', version);
        const appUrl = new URL('${proxiedPlaygroundBase}', window.location.origin);
        // Use a real query param on the playground URL so the iframe does a full navigation.
        appUrl.searchParams.set('reload', version);
        appUrl.hash = 'url=' + encodeURIComponent(fileUrl.toString());
        return appUrl.toString();
      }

      async function refreshVersion(initial = false) {
        const response = await fetch('/version', { cache: 'no-store' });
        if (!response.ok) {
          throw new Error('Failed to read version endpoint');
        }

        const payload = await response.json();
        updatedAtEl.textContent = new Date(payload.mtimeMs).toLocaleTimeString();

        if (payload.version !== currentVersion) {
          currentVersion = payload.version;
          iframe.src = buildIframeSrc(currentVersion);
          statusEl.textContent = initial ? 'Loaded' : 'Reloaded after file change';
        } else {
          statusEl.textContent = 'Watching for changes';
        }
      }

      async function tick(initial = false) {
        try {
          await refreshVersion(initial);
        } catch (error) {
          statusEl.textContent = 'Error: ' + error.message;
        }
      }

      tick(true);
      setInterval(() => tick(false), 1000);
    </script>
  </body>
</html>`;
}

function sendEmpty(res, status) {
  res.writeHead(status, {
    'Cache-Control': 'no-store',
  });
  res.end();
}

function escapeHtml(input) {
  return input
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url ?? '/', `http://${req.headers.host ?? `127.0.0.1:${port}`}`);

  try {
    if (req.method === 'GET' && url.pathname === '/favicon.ico') {
      sendEmpty(res, 204);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/') {
      sendHtml(res, buildHtml());
      return;
    }

    if (req.method === 'GET' && url.pathname === '/version') {
      sendJson(res, 200, await getFileInfo());
      return;
    }

    if (req.method === 'GET' && url.pathname === scadRoute) {
      const source = await readScadFile();
      res.writeHead(200, {
        'Content-Type': 'text/plain; charset=utf-8',
        'Cache-Control': 'no-store',
        'Access-Control-Allow-Origin': '*',
      });
      res.end(source);
      return;
    }

    if (req.method === 'GET' && url.pathname === '/openscad2') {
      res.writeHead(302, { Location: '/openscad2/' });
      res.end();
      return;
    }

    if (req.method === 'GET' && url.pathname.startsWith('/openscad2/')) {
      const upstreamPath = url.pathname.substring('/openscad2'.length) || '/';
      const upstreamUrl = new URL(`${hostedPlaygroundPath}${upstreamPath}${url.search}`, hostedPlayground);
      const upstream = await fetch(upstreamUrl, {
        headers: {
          'User-Agent': 'touch_clock-openscad-preview',
        },
      });

      const body = Buffer.from(await upstream.arrayBuffer());
      const contentType = upstream.headers.get('content-type') ?? 'application/octet-stream';
      const cacheControl = upstream.headers.get('cache-control') ?? 'public, max-age=3600';

      res.writeHead(upstream.status, {
        'Content-Type': contentType,
        'Cache-Control': cacheControl,
      });
      res.end(body);
      return;
    }

    sendJson(res, 404, { error: 'Not found' });
  } catch (error) {
    sendJson(res, 500, {
      error: error instanceof Error ? error.message : String(error),
      file: scadPath,
    });
  }
});

server.listen(port, '127.0.0.1', async () => {
  const info = await getFileInfo().catch(() => null);
  console.log(`OpenSCAD preview server running at http://127.0.0.1:${port}/`);
  console.log(`Serving ${scadPath}`);
  if (info) {
    console.log(`Current version ${info.version}`);
  }
});
