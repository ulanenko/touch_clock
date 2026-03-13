# OpenSCAD Browser Preview

This helper runs a local page that embeds `openscad-playground` and serves a local `.scad` file from this repo.

What it does:

- Opens your model in the browser through OpenSCAD Playground
- Watches the local `.scad` file for changes
- Reloads the embedded preview automatically after you save

Run it from the repo root:

```bash
node scripts/openscad-playground-preview.mjs 3d_model.scad 4174
```

Then open:

```text
http://127.0.0.1:4174/
```

Notes:

- The first argument is the SCAD file path, relative to the repo root
- The second argument is the local port
- One manual browser refresh is only needed after changing the preview script itself, not after editing the SCAD file
