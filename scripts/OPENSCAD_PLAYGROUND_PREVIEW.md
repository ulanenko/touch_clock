# OpenSCAD Browser Preview

This helper runs a local page that embeds `openscad-playground` and serves `.scad` files from this repo.

What it does:

- Opens your model in the browser through OpenSCAD Playground
- Watches the active local `.scad` file for changes
- Lets you switch between repo `.scad` files from the page without restarting the server
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

- The first argument is the default SCAD file path, relative to the repo root
- The second argument is the local port
- Use the file picker in the page header to switch between available `.scad` files
- One manual browser refresh is only needed after changing the preview script itself, not after editing the SCAD file
