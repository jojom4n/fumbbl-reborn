#!/usr/bin/env python3
"""Round 13 turn 4 — install a generated stadium into the client assets:

  1. detect the FIELD PIT (flood fill of the pit floor color from the
     image center) — the pit must be ~1.73:1 so the 26x15 field (square
     cells) fills it with no apron;
  2. compute fieldPit = the exact 26:15 rect centered in the pit;
  3. write the manifest (stadium.json) with normalized rects;
  4. upscale the image 2x (LANCZOS) -> 4K-ready asset;
  5. copy image + manifest to assets/stadiums/<name>/.

Also prints a grid overlay + top-band crop so the dugout rects can be
measured manually (the pavilions are wood: the flood fill does not reach
them through the frame).

Usage:
    python3 stadium_install.py <render.png> <stadium_name>
"""
import json
import os
import sys
from collections import deque
from PIL import Image

SRC = sys.argv[1]
NAME = sys.argv[2]
OUT = f"/home/saruman/Projects/FUMBBL-Reborn/assets/stadiums/{NAME}"


def detect_pit(im):
    px = im.load()
    w, h = im.size
    seed = px[w // 2, h // 2]
    r0, g0, b0 = seed
    # The pit floor is a flat color; tolerance for the painterly noise.
    def same(c):
        r, g, b = c
        return abs(r - r0) < 26 and abs(g - g0) < 26 and abs(b - b0) < 26

    seen = [[False] * w for _ in range(h)]
    q = deque([(w // 2, h // 2)])
    seen[h // 2][w // 2] = True
    minx, miny, maxx, maxy = w, h, 0, 0
    n = 0
    while q:
        x, y = q.popleft()
        n += 1
        minx = min(minx, x); maxx = max(maxx, x)
        miny = min(miny, y); maxy = max(maxy, y)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and same(px[nx, ny]):
                seen[ny][nx] = True
                q.append((nx, ny))
    return (minx, miny, maxx, maxy, seed, n)


def main():
    im = Image.open(SRC).convert("RGB")
    w, h = im.size
    pit = detect_pit(im)
    minx, miny, maxx, maxy, seed, n = pit
    pw, ph = maxx - minx, maxy - miny
    print(f"image: {w}x{h}")
    print(f"pit seed color: {seed}, flood pixels: {n}")
    print(f"pit rect px: ({minx},{miny})..({maxx},{maxy})  {pw}x{ph}")
    print(f"pit ratio: {pw/ph:.3f} (target 1.733)")

    # fieldPit: exact 26:15 centered in the pit.
    if pw / ph > 26 / 15:
        # pit too wide: fit by height
        fh = ph
        fw = fh * 26 / 15
    else:
        fw = pw
        fh = fw * 15 / 26
    fx = (minx + maxx - fw) / 2
    fy = (miny + maxy - fh) / 2
    apron_x = (pw - fw) / 2
    apron_y = (ph - fh) / 2
    print(f"fieldPit px: ({fx:.0f},{fy:.0f}) {fw:.0f}x{fh:.0f}  "
          f"apron x={apron_x:.0f}px y={apron_y:.0f}px")
    if apron_x > pw * 0.05 or apron_y > ph * 0.05:
        print("WARNING: apron > 5% — the pit ratio is off, consider another seed")

    # Grid overlay + top band crop for the manual dugout measurement.
    from PIL import ImageDraw
    g = im.copy()
    d = ImageDraw.Draw(g)
    for i in range(1, 20):
        x = w * i // 20
        d.line([(x, 0), (x, h)], fill=(255, 0, 255), width=1)
        d.text((x + 2, 5), str(i * 5), fill=(255, 0, 255))
        y = h * i // 20
        d.line([(0, y), (w, y)], fill=(0, 255, 255), width=1)
        d.text((5, y + 2), str(i * 5), fill=(0, 255, 255))
    # Mark the fieldPit
    d.rectangle([fx, fy, fx + fw, fy + fh], outline=(255, 255, 0), width=3)
    g.resize((w // 2, h // 2), Image.LANCZOS).save(f"/tmp/{NAME}_grid.png")
    im.crop((0, 0, w, int(h * 0.22))).save(f"/tmp/{NAME}_top.png")
    print(f"grid -> /tmp/{NAME}_grid.png   top -> /tmp/{NAME}_top.png")


if __name__ == "__main__":
    main()
