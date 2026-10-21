#!/usr/bin/env python3
"""
The Deserter: placeholder player sprite, drawn pixel by pixel.

Reference: docs/concept/deserter-sheet.png (AI-generated concept, not shipped).
This sprite is a hand-authored placeholder until the final art is drawn.

Run:  python3 tools/sprites/deserter.py
Out:  data/sprites/deserter.png  (sheet of 16x24 frames)
      rows    = facing: down (front), up (back), right (side; left is mirrored)
      columns = frame:  idle, step A, step B

No outline pixels: the game draws a 1-pixel dark outline around every sprite,
the same way Gungeon does.
"""
import pathlib
from PIL import Image

W, H = 16, 24

# Palette: the 8 colours from the concept sheet, plus derived shades and the
# skin tone and highlight the sheet was missing.
PALETTE = {
    ".": None,
    "B": "#354667",  # cloak blue (sheet)
    "b": "#26324D",  # cloak shadow
    "L": "#4D6596",  # cloak highlight
    "S": "#C9976F",  # weathered skin
    "s": "#8E6247",  # skin in the hood's shadow
    "Z": "#5E4535",  # stubble
    "H": "#322724",  # hair, eyes (sheet)
    "T": "#97856F",  # tunic (sheet)
    "t": "#514235",  # trousers, dark tunic (sheet)
    "K": "#6A4C38",  # leather (sheet)
    "G": "#C29548",  # gold clasp, buckle (sheet)
    "R": "#8D5238",  # rust, scar (sheet)
    "D": "#3A3B40",  # boots (sheet)
}

FRONT = """
......BBBB......
....BBBBBBBB....
...BLLBBBBBBB...
..BLBBBBBBBBBB..
..BBBBBBBBBBBB..
.BBBBbbbbbbBBBB.
.BBBbHHHHHHbBBB.
.BBbssssssssbBB.
.BBbsHHssHHsbBB.
.bBbSSSSSSRSbBb.
.bBbSSSssSSSbBb.
.bBBbZZZZZZbBBb.
.bbBBBBGGBBBBbb.
.bBBBtTTTTtBBBb.
bBBBBtTTTTtBBBBb
bBbBBKKGKKKBBbBb
bBBBBtTTTTtBBBBb
bbBBBttTTttBBbbb
.b.bBtt..ttBb.b.
....b.t..t.b....
"""

BACK = """
......BBBB......
....BBBBBBBB....
...BBBBBBBBLB...
..BBBBBBBBBBLB..
..BBBBBBBBBBBB..
.BBBBBBbBBBBBBB.
.BBBBBbbBBBBBBB.
.BBBBBBBBBBBBBB.
.BBBBBBBBBBBBBB.
.bBBBBBBBBBBBBb.
.bBBBBBBBBBBBBb.
.bbBBBBBBBBBBbb.
.bBBBBBBBBBKBBb.
.bBBBBBBBBKBBBb.
bBBBBBBBBKBBBBBb
bBBBBBBBKKKBBBBb
bBBBBbBBKRKBBBBb
bbBBBBBBBKBBBBbb
.bBbBBbBBBBbBbB.
..b.bBb..bBb.b..
"""

SIDE = """
.....BBBB.......
...BBBBBBBB.....
..BBLLBBBBBB....
.BBLBBBBBBBBB...
.BBBBBBBBBBBBB..
.BBBBBBBbbbbbB..
.BBBBBBbHHssb...
.BBBBBBbHsHHs...
.bBBBBBBbSSSSs..
.bBBBBBBbSSsS...
.bBBBBBBBZZZ....
..bBBBBBBBBGB...
..bBBBBBBBBBB...
.bBBBBBBBtTTB...
.bBBBBBBBtTTB...
.bBBBBBBKKGKB...
.bBbBBBBBtTTB...
bbBBBBBBBttTb...
b.bBbB.bBtt.b...
..b....b.t......
"""

LEGS_FRONT = [
    # idle
    """
.....tt..tt.....
.....tt..tt.....
.....DD..DD.....
....DDD..DDD....
""",
    # step A: left foot lifted
    """
.....tt..tt.....
.....DD..tt.....
....DDD..DD.....
.........DDD....
""",
    # step B: right foot lifted
    """
.....tt..tt.....
.....tt..DD.....
.....DD..DDD....
....DDD.........
""",
]

LEGS_SIDE = [
    """
.......tt.......
.......tt.......
.......DD.......
.......DDD......
""",
    """
......tt.t......
.....tt...t.....
.....DD...DD....
....DD.....DD...
""",
    """
.......tt.......
......t..t......
......DD.DD.....
.....DD...DD....
""",
]


def grid(text, rows):
    lines = [l for l in text.strip("\n").split("\n")]
    assert len(lines) == rows, f"expected {rows} rows, got {len(lines)}"
    for i, l in enumerate(lines):
        assert len(l) == W, f"row {i} is {len(l)} wide: {l!r}"
        for ch in l:
            assert ch in PALETTE, f"unknown colour {ch!r} in row {i}"
    return lines


def rgba(code):
    if code is None:
        return (0, 0, 0, 0)
    return tuple(int(code[i:i + 2], 16) for i in (1, 3, 5)) + (255,)


def frame(body, legs):
    img = Image.new("RGBA", (W, H))
    for y, line in enumerate(grid(body, 20) + grid(legs, 4)):
        for x, ch in enumerate(line):
            img.putpixel((x, y), rgba(PALETTE[ch]))
    return img


def main():
    rows = [(FRONT, LEGS_FRONT), (BACK, LEGS_FRONT), (SIDE, LEGS_SIDE)]
    sheet = Image.new("RGBA", (W * 3, H * len(rows)))
    for r, (body, legs) in enumerate(rows):
        for c, leg in enumerate(legs):
            sheet.paste(frame(body, leg), (c * W, r * H))
    root = pathlib.Path(__file__).resolve().parents[2]
    out = root / "data" / "sprites" / "deserter.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(f"wrote {out.relative_to(root)} ({sheet.width}x{sheet.height})")


if __name__ == "__main__":
    main()
