#!/usr/bin/env python3
"""Round 12 — re-style the block dice to the d6 visual language (v2, clean):
the big white circle becomes FLAT square-red (invisible), the red glyph
becomes pure WHITE with its original AA kept, and the small white holes
inside the glyph (skull eyes, tooth gaps, ...) become red so they stay
visible on the white glyph.

In:  /tmp/dice_backup/block{1..6}.png  (original 256px)
Out: assets/dice/block{1..6}.png       (replaced)
     assets/dice/block{1..6}_s.png     (re-generated 28px LANCZOS)
"""
import sys
from PIL import Image

ROOT = "/home/saruman/Projects/FUMBBL-Reborn/assets/dice"
BACKUP = "/tmp/dice_backup"
WHITE = (252, 253, 254)


def components(mask):
    h, w = len(mask), len(mask[0])
    labels = [[-1] * w for _ in range(h)]
    comps = []
    for y in range(h):
        for x in range(w):
            if mask[y][x] and labels[y][x] == -1:
                cid = len(comps)
                stack = [(x, y)]
                labels[y][x] = cid
                pix = []
                while stack:
                    cx, cy = stack.pop()
                    pix.append((cx, cy))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < w and 0 <= ny < h and mask[ny][nx] and labels[ny][nx] == -1:
                            labels[ny][nx] = cid
                            stack.append((nx, ny))
                comps.append(pix)
    return comps


def is_white_px(c):
    return min(c) > 200


def is_red_px(c):
    return c[0] > 110 and c[1] < 70 and c[2] < 70


def main():
    for n in range(1, 7):
        im = Image.open(f"{BACKUP}/block{n}.png").convert("RGB")
        w, h = im.size
        px = im.load()

        # flat square red = median of the mid-edge red pixels
        edge_reds = [px[10, h // 2], px[w - 11, h // 2], px[w // 2, 10], px[w // 2, h - 11]]
        flats = sorted(edge_reds, key=lambda c: c[0] + c[1] + c[2])
        flat_red = flats[1]

        white_mask = [[is_white_px(px[x, y]) for x in range(w)] for y in range(h)]
        red_mask = [[is_red_px(px[x, y]) for x in range(w)] for y in range(h)]

        # glyph = red components NOT touching the image border
        glyph = set()
        for comp in components(red_mask):
            if not any(x == 0 or y == 0 or x == w - 1 or y == h - 1 for x, y in comp):
                glyph.update(comp)
        assert glyph, "no enclosed red glyph"

        # circle bbox
        wcomps = components(white_mask)
        circle = max(wcomps, key=len)
        xs = [p[0] for p in circle]
        ys = [p[1] for p in circle]
        x0, x1 = min(xs), max(xs) + 1
        y0, y1 = min(ys), max(ys) + 1

        # AA pixels adjacent to the glyph (8-neighborhood) — keep their blend
        glyph_aa = set()
        for (gx, gy) in glyph:
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    nx, ny = gx + dx, gy + dy
                    if x0 <= nx < x1 and y0 <= ny < y1:
                        c = px[nx, ny]
                        if not is_white_px(c) and not is_red_px(c):
                            glyph_aa.add((nx, ny))

        for y in range(y0, y1):
            for x in range(x0, x1):
                if (x, y) in glyph:
                    px[x, y] = WHITE
                elif (x, y) in glyph_aa:
                    continue  # keep original red<->white blend
                else:
                    px[x, y] = flat_red  # circle whites, holes, AA fringe -> flat red

        im.save(f"{ROOT}/block{n}.png")
        im.resize((28, 28), Image.LANCZOS).save(f"{ROOT}/block{n}_s.png")
        print(f"block{n}: done (glyph={len(glyph)} aa={len(glyph_aa)})", file=sys.stderr)


if __name__ == "__main__":
    main()
