#!/usr/bin/env python3
"""Generate UI assets (PNG) for the Pokedex home screen.

Outputs into firmware/main/assets so the same images feed both the SDL host
preview and the Tab5 firmware (both draw PNGs through the Display interface).
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(__file__), "..", "firmware", "main", "assets")
os.makedirs(OUT, exist_ok=True)

POKE_YELLOW = (255, 203, 5, 255)
POKE_BLUE = (42, 117, 187, 255)
POKE_RED = (238, 21, 21, 255)
WHITE = (255, 255, 255, 255)
FONT_PATH = "/usr/share/fonts/TTF/NotoSans-Bold.ttf"


def make_title(text="SOLEDEX", font_size=120, stroke=8):
    font = ImageFont.truetype(FONT_PATH, font_size)
    # Measure including the stroke.
    tmp = Image.new("RGBA", (10, 10))
    d = ImageDraw.Draw(tmp)
    box = d.textbbox((0, 0), text, font=font, stroke_width=stroke)
    w = box[2] - box[0]
    h = box[3] - box[1]
    pad = 20
    img = Image.new("RGBA", (w + 2 * pad, h + 2 * pad), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Origin compensates for the bbox offset so nothing is clipped.
    d.text((pad - box[0], pad - box[1]), text, font=font,
           fill=POKE_YELLOW, stroke_width=stroke, stroke_fill=POKE_BLUE)
    path = os.path.join(OUT, "title_soledex.png")
    img.save(path)
    print("wrote", path, img.size)


def make_hero(size=300):
    # Scale the full-res Jolteon artwork down to fit the home screen stack.
    src = os.path.join(OUT, "jolteon.png")
    im = Image.open(src).convert("RGBA").resize((size, size), Image.LANCZOS)
    path = os.path.join(OUT, "jolteon_mid.png")
    im.save(path)
    print("wrote", path, im.size)


def make_mic_button(size=180):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Red circular button.
    d.ellipse([0, 0, size - 1, size - 1], fill=POKE_RED)
    cx = size / 2
    # Microphone capsule (body).
    body_w = size * 0.22
    body_top = size * 0.26
    body_bot = size * 0.56
    d.rounded_rectangle([cx - body_w / 2, body_top, cx + body_w / 2, body_bot],
                        radius=body_w / 2, fill=WHITE)
    # Cradle: a U-shaped arc hugging the lower half of the body.
    cradle_w = size * 0.40
    cradle_top = size * 0.40
    cradle_bot = size * 0.66
    d.arc([cx - cradle_w / 2, cradle_top, cx + cradle_w / 2, cradle_bot],
          start=20, end=160, fill=WHITE, width=max(2, int(size * 0.035)))
    # Stem and base.
    stem_w = max(2, int(size * 0.035))
    d.line([cx, cradle_bot, cx, size * 0.76], fill=WHITE, width=stem_w)
    base_w = size * 0.20
    d.line([cx - base_w / 2, size * 0.76, cx + base_w / 2, size * 0.76],
           fill=WHITE, width=stem_w)
    path = os.path.join(OUT, "mic_button.png")
    img.save(path)
    print("wrote", path, img.size)


def make_keyboard_button(size=180):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Blue circular button (complements the red mic above it).
    d.ellipse([0, 0, size - 1, size - 1], fill=POKE_BLUE)
    # White keyboard body: a rounded rectangle with a grid of keys.
    bx0, by0 = size * 0.22, size * 0.34
    bx1, by1 = size * 0.78, size * 0.66
    d.rounded_rectangle([bx0, by0, bx1, by1], radius=size * 0.05, fill=WHITE)
    # Punch out the keys as blue holes: 3 rows x 6 cols, plus a spacebar.
    cols, rows = 6, 3
    pad = size * 0.035
    gx0, gy0 = bx0 + pad, by0 + pad
    gx1, gy1 = bx1 - pad, by1 - pad
    kw = (gx1 - gx0) / cols
    kh = (gy1 - gy0) / rows
    inset = size * 0.012
    for r in range(rows):
        for c in range(cols):
            if r == rows - 1 and 1 <= c <= 4:
                continue  # leave the bottom-middle open for the spacebar
            x = gx0 + c * kw
            y = gy0 + r * kh
            d.rectangle([x + inset, y + inset, x + kw - inset, y + kh - inset],
                        fill=POKE_BLUE)
    # Spacebar (one wide key on the bottom row).
    sx0 = gx0 + 1 * kw + inset
    sx1 = gx0 + 5 * kw - inset
    sy0 = gy0 + 2 * kh + inset
    sy1 = gy0 + 3 * kh - inset
    d.rectangle([sx0, sy0, sx1, sy1], fill=POKE_BLUE)
    path = os.path.join(OUT, "keyboard_button.png")
    img.save(path)
    print("wrote", path, img.size)


if __name__ == "__main__":
    make_title()
    make_hero()
    make_mic_button()
    make_keyboard_button()
