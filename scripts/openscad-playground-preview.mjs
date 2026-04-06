#!/usr/bin/env node

import http from 'node:http';
import { createHash } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceRoot = path.resolve(__dirname, '..');

const requestedPath = process.argv[2] ?? '3d_model.scad';
const port = Number(process.argv[3] ?? process.env.PORT ?? 4173);
const hostedPlayground = 'https://ochafik.com/openscad2/';
const hostedPlaygroundPath = '/openscad2';
const proxiedPlaygroundBase = '/openscad2/';

function normalizeRelativePath(inputPath) {
  return inputPath.split(path.sep).join('/');
}

function ensureInsideWorkspace(absolutePath) {
  return absolutePath === workspaceRoot || absolutePath.startsWith(`${workspaceRoot}${path.sep}`);
}

async function listScadFiles(dir = workspaceRoot) {
  const entries = await fs.readdir(dir, { withFileTypes: true });
  const files = [];

  for (const entry of entries) {
    const absolutePath = path.join(dir, entry.name);

    if (entry.isDirectory()) {
      files.push(...await listScadFiles(absolutePath));
      continue;
    }

    if (entry.isFile() && entry.name.endsWith('.scad')) {
      files.push(normalizeRelativePath(path.relative(workspaceRoot, absolutePath)));
    }
  }

  return files.sort();
}

async function resolveScadFile(requestedFile = requestedPath) {
  const relativePath = normalizeRelativePath(requestedFile);
  const absolutePath = path.resolve(workspaceRoot, relativePath);

  if (!relativePath.endsWith('.scad') || !ensureInsideWorkspace(absolutePath)) {
    throw new Error(`Invalid SCAD path: ${requestedFile}`);
  }

  const stats = await fs.stat(absolutePath);
  if (!stats.isFile()) {
    throw new Error(`SCAD file not found: ${requestedFile}`);
  }

  return {
    relativePath,
    absolutePath,
    route: '/source',
    stats,
  };
}

async function getFileInfo(requestedFile) {
  const file = await resolveScadFile(requestedFile);
  return {
    relativePath: file.relativePath,
    size: file.stats.size,
    mtimeMs: Math.trunc(file.stats.mtimeMs),
    version: `${Math.trunc(file.stats.mtimeMs)}-${file.stats.size}`,
  };
}

async function getProjectInfo(requestedFile) {
  const [activeFile, availableFiles] = await Promise.all([
    resolveScadFile(requestedFile),
    listScadFiles(),
  ]);
  const fileInfos = await Promise.all(availableFiles.map(async (relativePath) => {
    const file = await resolveScadFile(relativePath);
    return {
      relativePath,
      size: file.stats.size,
      mtimeMs: Math.trunc(file.stats.mtimeMs),
    };
  }));

  const versionHash = createHash('sha1');
  for (const fileInfo of fileInfos) {
    versionHash.update(`${fileInfo.relativePath}:${fileInfo.mtimeMs}:${fileInfo.size}\n`);
  }

  return {
    relativePath: activeFile.relativePath,
    mtimeMs: Math.max(...fileInfos.map((fileInfo) => fileInfo.mtimeMs)),
    version: versionHash.digest('hex'),
  };
}

async function getProjectState(requestedFile) {
  const [activeFile, availableFiles] = await Promise.all([
    resolveScadFile(requestedFile),
    listScadFiles(),
  ]);
  const sources = await Promise.all(availableFiles.map(async (relativePath) => ({
    path: `/${relativePath}`,
    content: await readScadFile(relativePath),
  })));

  return {
    relativePath: activeFile.relativePath,
    state: {
      params: {
        activePath: `/${activeFile.relativePath}`,
        features: ['lazy-union'],
        sources,
      },
      view: {
        layout: {
          mode: 'multi',
          editor: true,
          viewer: true,
          customizer: false,
        },
      },
    },
  };
}

async function readScadFile(requestedFile) {
  const file = await resolveScadFile(requestedFile);
  return fs.readFile(file.absolutePath, 'utf8');
}

function resolveSourcePath(sourcePath) {
  const normalizedPath = path.posix.normalize(sourcePath);
  const relativePath = normalizedPath.replace(/^\/+/, '');

  if (!relativePath || relativePath.startsWith('..') || !relativePath.endsWith('.scad')) {
    throw new Error(`Invalid source path: ${sourcePath}`);
  }

  return relativePath;
}

async function writeProjectSources(projectState, expectedVersion) {
  if (!projectState || typeof projectState !== 'object') {
    throw new Error('Invalid project state payload');
  }

  const sources = projectState.params?.sources;
  if (!Array.isArray(sources) || sources.length === 0) {
    throw new Error('Project payload must include sources');
  }

  const activePath = projectState.params?.activePath;
  if (typeof activePath !== 'string') {
    throw new Error('Project payload must include activePath');
  }

  const normalizedActivePath = resolveSourcePath(activePath);
  const currentInfo = await getProjectInfo(normalizedActivePath);
  if (expectedVersion && currentInfo.version !== expectedVersion) {
    const error = new Error('Local files changed since the browser state was loaded');
    error.statusCode = 409;
    throw error;
  }

  for (const source of sources) {
    if (!source || typeof source !== 'object') {
      throw new Error('Invalid source entry');
    }

    const relativePath = resolveSourcePath(source.path);
    if (typeof source.content !== 'string') {
      throw new Error(`Source content missing for ${relativePath}`);
    }

    const file = await resolveScadFile(relativePath);
    await fs.writeFile(file.absolutePath, source.content, 'utf8');
  }

  return getProjectInfo(normalizedActivePath);
}

async function readJsonBody(req) {
  const chunks = [];

  for await (const chunk of req) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }

  const rawBody = Buffer.concat(chunks).toString('utf8');
  return rawBody ? JSON.parse(rawBody) : {};
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

function buildHtml(activeFile, availableFiles) {
  const fileOptions = availableFiles
    .map((file) => {
      const selected = file === activeFile.relativePath ? ' selected' : '';
      return `<option value="${escapeHtml(file)}"${selected}>${escapeHtml(file)}</option>`;
    })
    .join('');

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
      select {
        font: inherit;
        color: inherit;
        border: 1px solid rgba(26, 26, 26, 0.12);
        background: white;
        border-radius: 999px;
        padding: 0.35rem 0.65rem;
      }
      button {
        font: inherit;
        color: inherit;
        border: 1px solid rgba(26, 26, 26, 0.12);
        background: white;
        border-radius: 999px;
        padding: 0.35rem 0.75rem;
        cursor: pointer;
      }
      button:disabled {
        opacity: 0.6;
        cursor: progress;
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
        <label class="pill"><strong>SCAD:</strong>
          <select id="file-picker" aria-label="Select SCAD file">
            ${fileOptions}
          </select>
        </label>
        <button id="save-button" type="button">Save Local</button>
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
      const filePicker = document.getElementById('file-picker');
      const saveButton = document.getElementById('save-button');
      const statusEl = document.getElementById('status');
      const updatedAtEl = document.getElementById('updated-at');
      let currentVersion = null;
      let activeFile = ${JSON.stringify(activeFile.relativePath)};

      function currentPageUrl() {
        const pageUrl = new URL(window.location.href);
        pageUrl.searchParams.set('file', activeFile);
        return pageUrl;
      }

      function buildIframeSrc(payload) {
        const appUrl = new URL('${proxiedPlaygroundBase}', window.location.origin);
        // Use a real query param on the playground URL so the iframe does a full navigation.
        appUrl.searchParams.set('reload', payload.version);
        appUrl.hash = encodeURIComponent(JSON.stringify(payload.state));
        return appUrl.toString();
      }

      async function parseIframeState() {
        const rawHash = iframe.contentWindow?.location?.hash ?? '';
        const hash = rawHash.startsWith('#') ? rawHash.slice(1) : rawHash;

        if (!hash) {
          throw new Error('Preview state is not ready yet');
        }

        try {
          return JSON.parse(decodeURIComponent(hash));
        } catch (decodeError) {
          try {
            const binary = Uint8Array.from(atob(hash), (char) => char.charCodeAt(0));
            const stream = new ReadableStream({
              start(controller) {
                controller.enqueue(binary);
                controller.close();
              },
            }).pipeThrough(new DecompressionStream('gzip'));
            return JSON.parse(await new Response(stream).text());
          } catch {
            throw new Error('Failed to read current editor state');
          }
        }
      }

      async function saveLocalChanges() {
        saveButton.disabled = true;
        statusEl.textContent = 'Saving local files…';

        try {
          const state = await parseIframeState();
          const response = await fetch('/save-project', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json; charset=utf-8',
            },
            body: JSON.stringify({
              expectedVersion: currentVersion,
              state,
            }),
          });
          const payload = await response.json().catch(() => ({}));
          if (!response.ok) {
            throw new Error(payload.error || 'Save failed');
          }

          currentVersion = payload.version ?? currentVersion;
          if (payload.mtimeMs) {
            updatedAtEl.textContent = new Date(payload.mtimeMs).toLocaleTimeString();
          }
          statusEl.textContent = 'Saved locally';
        } catch (error) {
          statusEl.textContent = 'Save failed: ' + error.message;
        } finally {
          saveButton.disabled = false;
        }
      }

      async function refreshVersion(initial = false) {
        const versionUrl = currentPageUrl();
        versionUrl.pathname = '/version';
        const response = await fetch(versionUrl, { cache: 'no-store' });
        if (!response.ok) {
          throw new Error('Failed to read version endpoint');
        }

        const payload = await response.json();
        updatedAtEl.textContent = new Date(payload.mtimeMs).toLocaleTimeString();
        filePicker.value = payload.relativePath;

        if (payload.version !== currentVersion) {
          currentVersion = payload.version;
          const projectUrl = currentPageUrl();
          projectUrl.pathname = '/project';
          const projectResponse = await fetch(projectUrl, { cache: 'no-store' });
          if (!projectResponse.ok) {
            throw new Error('Failed to read project endpoint');
          }

          iframe.src = buildIframeSrc(await projectResponse.json());
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

      filePicker.addEventListener('change', () => {
        activeFile = filePicker.value;
        const nextUrl = currentPageUrl();
        window.location.assign(nextUrl);
      });
      saveButton.addEventListener('click', () => {
        void saveLocalChanges();
      });

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
      const requestedFile = url.searchParams.get('file') ?? requestedPath;
      const [activeFile, availableFiles] = await Promise.all([
        resolveScadFile(requestedFile),
        listScadFiles(),
      ]);
      sendHtml(res, buildHtml(activeFile, availableFiles));
      return;
    }

    if (req.method === 'GET' && url.pathname === '/version') {
      sendJson(res, 200, await getProjectInfo(url.searchParams.get('file') ?? requestedPath));
      return;
    }

    if (req.method === 'GET' && url.pathname === '/project') {
      sendJson(res, 200, await getProjectState(url.searchParams.get('file') ?? requestedPath));
      return;
    }

    if (req.method === 'POST' && url.pathname === '/save-project') {
      const payload = await readJsonBody(req);
      sendJson(res, 200, await writeProjectSources(payload.state, payload.expectedVersion));
      return;
    }

    if (req.method === 'GET' && url.pathname === '/source') {
      const source = await readScadFile(url.searchParams.get('file') ?? requestedPath);
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
    sendJson(res, typeof error?.statusCode === 'number' ? error.statusCode : 500, {
      error: error instanceof Error ? error.message : String(error),
      file: url.searchParams.get('file') ?? requestedPath,
    });
  }
});

server.listen(port, '127.0.0.1', async () => {
  const info = await getFileInfo().catch(() => null);
  console.log(`OpenSCAD preview server running at http://127.0.0.1:${port}/`);
  console.log(`Default SCAD ${requestedPath}`);
  if (info) {
    console.log(`Current version ${info.version}`);
  }
});
