"""Export each 720x720 round screen of a concept page to compare/<slug>/NN.png.

Usage: python3 design/ui_concepts/export_screens.py instrument orbit atelier
Screens are found as 720x720 elements with a 50% border radius, in document order.
"""
import asyncio
import io
import sys
from pathlib import Path

from PIL import Image
from playwright.async_api import async_playwright

ROOT = Path(__file__).resolve().parent

# Index (in page order) of the screen used for each of the 12 shared screens, for pages
# that add extra states. Pages not listed use their first 12 screens.
SCREEN_ORDER = {
    "instrument": [0, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15],
    "atelier": [1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 14, 16],
}

MARK_SCREENS = """
() => {
  let i = 0;
  for (const el of document.querySelectorAll('body *')) {
    const cs = getComputedStyle(el);
    if (cs.width === '720px' && cs.height === '720px' && cs.borderTopLeftRadius === '50%') {
      if (!el.parentElement.closest('[data-export]')) el.setAttribute('data-export', String(i++));
    }
  }
  const first = document.querySelector('[data-export="0"]');
  return first ? first.getBoundingClientRect().width : 0;
}
"""


async def export(slug, browser):
    page = await browser.new_page(viewport={"width": 1600, "height": 1200})
    await page.goto((ROOT / f"{slug}.html").as_uri(), wait_until="networkidle")
    await page.evaluate("document.fonts.ready")
    width = await page.evaluate(MARK_SCREENS)
    count = await page.evaluate("document.querySelectorAll('[data-export]').length")
    await page.close()
    if not width:
        print(f"{slug}: no screens found")
        return

    scale = 720 / width
    context = await browser.new_context(viewport={"width": 1600, "height": 1200}, device_scale_factor=scale)
    page = await context.new_page()
    await page.goto((ROOT / f"{slug}.html").as_uri(), wait_until="networkidle")
    await page.evaluate("document.fonts.ready")
    await page.evaluate(MARK_SCREENS)
    out_dir = ROOT / "compare" / slug
    out_dir.mkdir(parents=True, exist_ok=True)
    order = SCREEN_ORDER.get(slug, list(range(min(count, 12))))
    for n, i in enumerate(order, start=1):
        el = page.locator(f'[data-export="{i}"]')
        await el.scroll_into_view_if_needed()
        await page.wait_for_timeout(150)
        png = await el.screenshot()
        img = Image.open(io.BytesIO(png)).convert("RGB").resize((720, 720), Image.LANCZOS)
        img.save(out_dir / f"{n:02d}.png")
    await context.close()
    print(f"{slug}: exported {len(order)} of {count} screens at scale {scale:.2f}")


async def main(slugs):
    async with async_playwright() as p:
        browser = await p.chromium.launch(channel="chrome")
        for slug in slugs:
            await export(slug, browser)
        await browser.close()


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1:] or ["instrument", "orbit", "atelier"]))
