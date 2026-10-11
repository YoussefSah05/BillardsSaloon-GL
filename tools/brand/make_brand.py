#!/usr/bin/env python3
"""Generates the Septhuit brand assets from code, so they can be rebuilt exactly.

The mark is the 7 and the 8 ball kissing; in the wordmark it stands in for the
ampersand of "Sept & Huit". Letters are converted to outlines from the bundled
Barlow Condensed fonts (SIL OFL), so the SVGs render the same everywhere.

Needs: python3 with fontTools and Pillow, rsvg-convert (librsvg), and iconutil
on macOS for the .icns. Run from anywhere:

    python3 tools/brand/make_brand.py
"""

from __future__ import annotations

import math
import shutil
import subprocess
import tempfile
from pathlib import Path

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
FONTS = ROOT / "assets" / "fonts"
BRAND = ROOT / "assets" / "brand"
MEDIA = ROOT / "docs" / "media"

# Palette from assets/ui/theme.rcss.
INK = "#111214"
CHALK = "#F2EEE6"
GOLD = "#D4A94A"
MUTED = "#A9A39A"
HALL = "#0B0D0F"
SEVEN = "#7B1E2B"   # the 7: maroon
EIGHT = "#141416"   # the 8: black

BALL_R = 100.0
KISS_ANGLE = math.radians(-14.0)   # the 8 sits slightly higher than the 7


class Font:
    def __init__(self, file: str):
        self.font = TTFont(FONTS / file)
        self.glyphs = self.font.getGlyphSet()
        self.cmap = self.font.getBestCmap()
        self.upem = self.font["head"].unitsPerEm
        self.cap = self.font["OS/2"].sCapHeight

    def text(self, text: str, cap_height: float, x: float, baseline: float, tracking: float = 0.0) -> tuple[str, float]:
        """SVG path data for text whose capitals are cap_height tall; returns (d, advance)."""
        scale = cap_height / self.cap
        d = []
        cursor = x
        for ch in text:
            name = self.cmap[ord(ch)]
            pen = SVGPathPen(self.glyphs)
            self.glyphs[name].draw(TransformPen(pen, (scale, 0, 0, -scale, cursor, baseline)))
            d.append(pen.getCommands())
            cursor += self.glyphs[name].width * scale + tracking * cap_height
        return " ".join(d), cursor - x - tracking * cap_height

    def ink_bounds(self, ch: str, cap_height: float) -> tuple[float, float, float, float]:
        scale = cap_height / self.cap
        pen = BoundsPen(self.glyphs)
        self.glyphs[self.cmap[ord(ch)]].draw(pen)
        x0, y0, x1, y1 = pen.bounds
        return x0 * scale, y0 * scale, x1 * scale, y1 * scale


DISPLAY = Font("BarlowCondensed-ExtraBold.ttf")
NUMERAL = Font("BarlowCondensed-Bold.ttf")
BODY = Font("Barlow-SemiBold.ttf")


def ball(cx: float, cy: float, colour: str, digit: str, uid: str) -> str:
    """A shaded pool ball: base colour, rim shadow, specular light, number disc."""
    disc_r = 0.45 * BALL_R
    cap = 0.56 * BALL_R
    x0, _, x1, _ = NUMERAL.ink_bounds(digit, cap)
    d, _ = NUMERAL.text(digit, cap, cx - (x0 + x1) / 2, cy + cap / 2)
    return f"""
  <g>
    <defs>
      <radialGradient id="{uid}-shade" cx="0.5" cy="0.5" r="0.5">
        <stop offset="0.62" stop-color="#000" stop-opacity="0"/>
        <stop offset="1" stop-color="#000" stop-opacity="0.45"/>
      </radialGradient>
      <radialGradient id="{uid}-light" cx="0.34" cy="0.28" r="0.42">
        <stop offset="0" stop-color="#fff" stop-opacity="0.55"/>
        <stop offset="1" stop-color="#fff" stop-opacity="0"/>
      </radialGradient>
    </defs>
    <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{BALL_R}" fill="{colour}"/>
    <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{BALL_R}" fill="url(#{uid}-shade)"/>
    <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{disc_r:.2f}" fill="{CHALK}"/>
    <path d="{d}" fill="{INK}"/>
    <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{BALL_R}" fill="url(#{uid}-light)"/>
    <circle cx="{cx:.2f}" cy="{cy:.2f}" r="{BALL_R - 1.5}" fill="none" stroke="{CHALK}" stroke-opacity="0.22" stroke-width="3"/>
  </g>"""


def spark(x: float, y: float, size: float) -> str:
    """A four-pointed gold star at the kiss."""
    s, w = size, size * 0.22
    d = (f"M{x:.2f},{y - s:.2f} L{x + w:.2f},{y - w:.2f} L{x + s:.2f},{y:.2f} L{x + w:.2f},{y + w:.2f} "
         f"L{x:.2f},{y + s:.2f} L{x - w:.2f},{y + w:.2f} L{x - s:.2f},{y:.2f} L{x - w:.2f},{y - w:.2f} Z")
    return f'<path d="{d}" fill="{GOLD}" stroke="{INK}" stroke-width="{size * 0.08:.2f}" stroke-linejoin="round"/>'


def mark(ox: float, oy: float, scale: float, uid: str) -> tuple[str, float, float]:
    """The 7 and the 8 kissing. (ox, oy) is the top-left of its bounding box; returns (svg, w, h)."""
    c7 = (BALL_R, BALL_R - 2 * BALL_R * math.sin(KISS_ANGLE))
    c8 = (c7[0] + 2 * BALL_R * math.cos(KISS_ANGLE), c7[1] + 2 * BALL_R * math.sin(KISS_ANGLE))
    kiss = ((c7[0] + c8[0]) / 2, (c7[1] + c8[1]) / 2)
    w = c8[0] + BALL_R
    h = c7[1] + BALL_R
    body = ball(*c7, SEVEN, "7", f"{uid}7") + ball(*c8, EIGHT, "8", f"{uid}8") + spark(*kiss, 0.34 * BALL_R)
    return f'<g transform="translate({ox:.2f},{oy:.2f}) scale({scale:.4f})">{body}</g>', w * scale, h * scale


def svg(width: float, height: float, body: str, background: str | None = None, title: str = "Septhuit") -> str:
    bg = f'<rect width="100%" height="100%" fill="{background}"/>' if background else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width:.0f} {height:.0f}" '
            f'width="{width:.0f}" height="{height:.0f}" role="img" aria-label="{title}">'
            f"<title>{title}</title>{bg}{body}</svg>\n")


def wordmark(text_colour: str, uid: str) -> tuple[str, float, float]:
    """SEPT [mark] HUIT: returns (body, width, height) in its own coordinates."""
    cap = 150.0
    gap = 0.22 * cap
    _, mark_w0, mark_h0 = mark(0, 0, 1, uid)
    mark_scale = 1.32 * cap / mark_h0
    mark_w, mark_h = mark_w0 * mark_scale, mark_h0 * mark_scale
    height = mark_h
    baseline = (height + cap) / 2
    left, left_w = DISPLAY.text("SEPT", cap, 0, baseline, tracking=0.04)
    mark_svg, _, _ = mark(left_w + gap, (height - mark_h) / 2, mark_scale, uid)
    right_x = left_w + 2 * gap + mark_w
    right, right_w = DISPLAY.text("HUIT", cap, right_x, baseline, tracking=0.04)
    body = f'<path d="{left} {right}" fill="{text_colour}"/>{mark_svg}'
    return body, right_x + right_w, height


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    print("wrote", path.relative_to(ROOT))


def rsvg(source: Path, target: Path, width: int, height: int | None = None) -> None:
    args = ["rsvg-convert", "-w", str(width)]
    if height:
        args += ["-h", str(height)]
    subprocess.run(args + ["-o", str(target), str(source)], check=True)


def main() -> None:
    pad = 24.0

    # The mark alone, and the app icon (mark on an ink tile).
    body, w, h = mark(pad, pad, 1, "m")
    write(BRAND / "mark.svg", svg(w + 2 * pad, h + 2 * pad, body))
    tile = 1024.0
    inner = 0.80 * tile
    _, mw, mh = mark(0, 0, 1, "i")
    s = inner / mw
    body, w, h = mark((tile - mw * s) / 2, (tile - mh * s) / 2, s, "i")
    radius = 0.22 * tile
    icon = (f'<rect width="{tile:.0f}" height="{tile:.0f}" rx="{radius:.0f}" fill="{INK}"/>'
            f'<rect x="14" y="14" width="{tile - 28:.0f}" height="{tile - 28:.0f}" rx="{radius - 14:.0f}" '
            f'fill="none" stroke="{GOLD}" stroke-width="10" stroke-opacity="0.85"/>{body}')
    write(BRAND / "icon.svg", svg(tile, tile, icon))

    # Wordmarks: chalk letters for dark backgrounds, ink letters for light ones.
    for name, colour in (("wordmark-dark", CHALK), ("wordmark-light", INK)):
        body, w, h = wordmark(colour, name)
        write(BRAND / f"{name}.svg", svg(w + 2 * pad, h + 2 * pad, f'<g transform="translate({pad},{pad})">{body}</g>'))

    # Social preview, 1280 x 640.
    body, w, h = wordmark(CHALK, "social")
    s = 900 / w
    tagline, tag_w = BODY.text("Tournament pool, simulated exactly.", 34, 0, 0)
    kicker, kick_w = DISPLAY.text("THE TOURNAMENT HALL", 22, 0, 0, tracking=0.5)
    preview = (
        f'<defs><radialGradient id="glow" cx="0.5" cy="0.38" r="0.75">'
        f'<stop offset="0" stop-color="#3a2a14"/><stop offset="0.55" stop-color="#16120d"/>'
        f'<stop offset="1" stop-color="{HALL}"/></radialGradient></defs>'
        f'<rect width="1280" height="640" fill="url(#glow)"/>'
        f'<path transform="translate({(1280 - kick_w) / 2:.1f},180)" d="{kicker}" fill="{GOLD}"/>'
        f'<g transform="translate({(1280 - w * s) / 2:.1f},{215:.1f}) scale({s:.4f})">{body}</g>'
        f'<rect x="610" y="{215 + h * s + 34:.1f}" width="60" height="4" fill="{GOLD}"/>'
        f'<path transform="translate({(1280 - tag_w) / 2:.1f},{215 + h * s + 100:.1f})" d="{tagline}" fill="{MUTED}"/>')
    social_svg = BRAND / "social-preview.svg"
    write(social_svg, svg(1280, 640, preview))

    if not shutil.which("rsvg-convert"):
        raise SystemExit("rsvg-convert not found (brew install librsvg); SVGs written, rasters skipped.")

    # In-game wordmark: RmlUi's GL3 backend loads uncompressed 32-bit TGA only.
    with tempfile.TemporaryDirectory() as tmp:
        png = Path(tmp) / "wordmark.png"
        rsvg(BRAND / "wordmark-dark.svg", png, 1400)   # sharp at the title size on HiDPI
        Image.open(png).convert("RGBA").save(BRAND / "wordmark.tga")
        print("wrote", (BRAND / "wordmark.tga").relative_to(ROOT))

        # Window icons (GLFW on Windows/Linux).
        for size in (16, 32, 48, 256):
            rsvg(BRAND / "icon.svg", BRAND / f"icon_{size}.png", size, size)
        print("wrote assets/brand/icon_{16,32,48,256}.png")

        # macOS icon set.
        if shutil.which("iconutil"):
            iconset = Path(tmp) / "Septhuit.iconset"
            iconset.mkdir()
            for size in (16, 32, 128, 256, 512):
                rsvg(BRAND / "icon.svg", iconset / f"icon_{size}x{size}.png", size, size)
                rsvg(BRAND / "icon.svg", iconset / f"icon_{size}x{size}@2x.png", 2 * size, 2 * size)
            subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(BRAND / "Septhuit.icns")], check=True)
            print("wrote assets/brand/Septhuit.icns")

    rsvg(social_svg, MEDIA / "social-preview.png", 1280, 640)
    print("wrote docs/media/social-preview.png")


if __name__ == "__main__":
    main()
