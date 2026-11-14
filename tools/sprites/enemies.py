#!/usr/bin/env python3
"""
Enemy body sprites: placeholders, drawn pixel by pixel.

Run:  python3 tools/sprites/enemies.py
Out:  data/sprites/enemies.png  (sheet of 16x16 frames)
      rows    = shape: round (the Grunt), square (the Scatter)
      columns = frame: stand, bob

The sprites are grey. The game multiplies them by the enemy's colour from
data/enemies.json, so a recoloured enemy needs no new art. The dark visor band
is where the game draws the eyes, which follow the player.
"""
import pathlib
from PIL import Image

W = H = 16
SHADES = {".": None, "W": 255, "L": 216, "M": 168, "D": 116, "K": 52}

ROUND = """
................
................
.....WWWWWW.....
....WLLLLLLM....
...WLLLLLLLLM...
...LLLLLLLLLM...
..MMMMMMMMMMMD..
...KKKKKKKKKK...
...KKKKKKKKKK...
...LLLLLLLLLM...
...LLLLLLLLMM...
...MLLLLLLMMD...
....MMMMMMDD....
....DD....DD....
....DD....DD....
................
"""

SQUARE = """
................
..MMMMMMMMMMMM..
..WWWWWWWWWWWM..
..WLLLLLLLLLLM..
..WLLLLLLLLLLM..
..LLLLLLLLLLLM..
..KKKKKKKKKKKK..
..KKKKKKKKKKKK..
..LLLLLLLLLLLM..
..LLMLLLLLMLLM..
..LLLLLLLLLLMM..
..MLLLLLLLLMMD..
..MMMMMMMMMMDD..
...DDD....DDD...
...DDD....DDD...
................
"""


def grid(text):
    lines = text.strip("\n").split("\n")
    assert len(lines) == H, f"expected {H} rows, got {len(lines)}"
    for i, l in enumerate(lines):
        assert len(l) == W, f"row {i} is {len(l)} wide: {l!r}"
        assert all(ch in SHADES for ch in l), f"unknown shade in row {i}"
    return lines


def bob(lines):
    """Second frame: the body sinks one pixel onto the feet."""
    return [lines[0], "." * W] + lines[1:12] + lines[13:]


def frame(lines):
    img = Image.new("RGBA", (W, H))
    for y, line in enumerate(lines):
        for x, ch in enumerate(line):
            v = SHADES[ch]
            img.putpixel((x, y), (0, 0, 0, 0) if v is None else (v, v, v, 255))
    return img


def main():
    shapes = [grid(ROUND), grid(SQUARE)]
    sheet = Image.new("RGBA", (W * 2, H * len(shapes)))
    for r, lines in enumerate(shapes):
        sheet.paste(frame(lines), (0, r * H))
        sheet.paste(frame(bob(lines)), (W, r * H))
    root = pathlib.Path(__file__).resolve().parents[2]
    out = root / "data" / "sprites" / "enemies.png"
    sheet.save(out)
    print(f"wrote {out.relative_to(root)} ({sheet.width}x{sheet.height})")


if __name__ == "__main__":
    main()
