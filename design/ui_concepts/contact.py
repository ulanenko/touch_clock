"""Contact sheet of compare/<slug>/01..12.png for a quick check."""
import sys
from pathlib import Path

from PIL import Image

root = Path(__file__).resolve().parent
slug, out = sys.argv[1], sys.argv[2]
sheet = Image.new("RGB", (6 * 240, 2 * 240))
for n in range(1, 13):
    path = root / "compare" / slug / f"{n:02d}.png"
    if path.exists():
        sheet.paste(Image.open(path).resize((240, 240)), (((n - 1) % 6) * 240, ((n - 1) // 6) * 240))
sheet.save(out)
