"""Draws the quest marker icons as PNG files, with no libraries beyond Python's own.

The marker is a gold triangle pointing down with a dark outline, like the markers players know from Skyrim. The
optional marker, for side steps the player can take or leave, is the same triangle, hollow, with its middle dark so
that a door marker under it doesn't show through. The arrows say
whether a target is above or below the player. Each shape is drawn four times larger and shrunk, so its
edges are smooth.

Run it from the repository root: python ui-rework/tools/make_quest_icons.py
"""

import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "files" / "data" / "textures" / "omw"
GOLD = (223, 190, 110)
DARK = (30, 22, 12)
SCALE = 4


def write_png(path, size, pixels):
    rows = b"".join(b"\0" + bytes(c for px in row for c in px) for row in pixels)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size[0], size[1], 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def inside_triangle(x, y, a, b, c):
    def side(p, q, r):
        return (p[0] - r[0]) * (q[1] - r[1]) - (q[0] - r[0]) * (p[1] - r[1])
    d1, d2, d3 = side((x, y), a, b), side((x, y), b, c), side((x, y), c, a)
    neg = d1 < 0 or d2 < 0 or d3 < 0
    pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def draw(size, shapes):
    """Draws shapes, each (corners, colour), the later ones on top, and returns the shrunk pixels."""
    w, h = size
    big = [[None] * (w * SCALE) for _ in range(h * SCALE)]
    for corners, colour in shapes:
        pts = [(x * SCALE, y * SCALE) for x, y in corners]
        for y in range(h * SCALE):
            for x in range(w * SCALE):
                if inside_triangle(x + 0.5, y + 0.5, *pts):
                    big[y][x] = colour
    pixels = []
    for y in range(h):
        row = []
        for x in range(w):
            samples = [big[y * SCALE + j][x * SCALE + i] for j in range(SCALE) for i in range(SCALE)]
            filled = [s for s in samples if s is not None]
            alpha = round(255 * len(filled) / len(samples))
            if not filled:
                row.append((0, 0, 0, 0))
                continue
            colour = tuple(round(sum(s[k] for s in filled) / len(filled)) for k in range(3))
            row.append(colour + (alpha,))
        pixels.append(row)
    return pixels


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # The marker: a triangle pointing down, outlined.
    write_png(OUT / "quest_marker.png", (32, 32), draw((32, 32), [
        (((2, 4), (30, 4), (16, 30)), DARK),
        (((6.5, 6.8), (25.5, 6.8), (16, 24.5)), GOLD),
    ]))
    # The optional marker: the same triangle, hollow, with a dark middle.
    write_png(OUT / "quest_marker_optional.png", (32, 32), draw((32, 32), [
        (((2, 4), (30, 4), (16, 30)), DARK),
        (((6.5, 6.8), (25.5, 6.8), (16, 24.5)), GOLD),
        (((10.4, 9.0), (21.6, 9.0), (16, 19.4)), DARK),
    ]))
    # The arrows: small triangles pointing up and down, outlined.
    write_png(OUT / "quest_arrow_up.png", (16, 16), draw((16, 16), [
        (((1, 14), (15, 14), (8, 2)), DARK),
        (((4, 12.3), (12, 12.3), (8, 5.5)), GOLD),
    ]))
    write_png(OUT / "quest_arrow_down.png", (16, 16), draw((16, 16), [
        (((1, 2), (15, 2), (8, 14)), DARK),
        (((4, 3.7), (12, 3.7), (8, 10.5)), GOLD),
    ]))


if __name__ == "__main__":
    main()
