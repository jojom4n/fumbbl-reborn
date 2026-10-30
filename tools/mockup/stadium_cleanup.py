#!/usr/bin/env python3
"""Round 13 turn 4c — clean a generated arena render for client use.

What it fixes (automated + iterative):
  1. PIT: the central floor must be a UNIFORM flat color (the client draws
     the grass field exactly over it — any texture/lines/grass left by the
     model shows as artifacts). Flood-fills the pit from the center, then
     fills the pit bbox with the mean pit color.
  2. WATERMARK: the reference's Patreon watermark (bottom-left) is removed
     by copying a nearby floor patch over it.
  3. SLOTS: the reference's numbered turn-slots (top band) are in the
     dugout zones — patched flat wood via --patch rects (iterative: look
     at the grid image, add rects, re-run).

Usage:
    python3 stadium_cleanup.py <render.png> <name> \
        [--patch x,y,w,h ...] [--patchsrc x,y,w,h fx,fy ...]
Outputs:
    /tmp/<name>_clean.png   the cleaned image (client-ready, 1536px)
    /tmp/<name>_grid.png    grid overlay for manifest measurement
"""
import argparse
import sys
from collections import deque
from PIL import Image, ImageDraw


def flood_bbox(im, tol=30):
    px = im.load()
    w, h = im.size
    r0, g0, b0 = px[w // 2, h // 2]
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
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx]:
                r, g, b = px[nx, ny]
                if (abs(r - r0) < tol and abs(g - g0) < tol and abs(b - b0) < tol):
                    seen[ny][nx] = True
                    q.append((nx, ny))
    return (minx, miny, maxx, maxy), n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("name")
    ap.add_argument("--patch", action="append", default=[],
                    help="x,y,w,h -> fill with local mean color")
    ap.add_argument("--patchsrc", action="append", default=[],
                    help="x,y,w,h fx,fy -> copy from (fx,fy)")
    ap.add_argument("--pitrect", default=None,
                    help="x,y,w,h -> pit bbox override (flood fill fails on "
                         "textured/grass pits: measure from the grid image)")
    ap.add_argument("--pitcolor", default=None,
                    help="r,g,b -> explicit pit fill color (default: sample)")
    args = ap.parse_args()

    im = Image.open(args.src).convert("RGB")
    w, h = im.size
    px = im.load()
    d = ImageDraw.Draw(im)

    # 1) Pit: flood from center (or the measured override), fill the bbox
    # with a UNIFORM color (the client's field covers the pit exactly).
    if args.pitrect:
        minx, miny, pw, ph = map(int, args.pitrect.split(","))
        maxx, maxy = minx + pw - 1, miny + ph - 1
        n = pw * ph
    else:
        (minx, miny, maxx, maxy), n = flood_bbox(im)
    print(f"pit bbox: ({minx},{miny})..({maxx},{maxy})  {maxx-minx}x{maxy-miny}  "
          f"ratio {(maxx-minx)/max(1,(maxy-miny)):.3f}")
    if args.pitcolor:
        mean = tuple(map(int, args.pitcolor.split(",")))
    else:
        rs = gs = bs = 0
        cnt = 0
        for y in range(miny, maxy + 1, 4):
            for x in range(minx, maxx + 1, 4):
                r, g, b = px[x, y]
                rs += r; gs += g; bs += b; cnt += 1
        mean = (rs // cnt, gs // cnt, bs // cnt)
    print(f"pit fill color: {mean}")
    d.rectangle([minx, miny, maxx, maxy], fill=mean)

    # 2) Watermark (bottom-left): copy a floor patch from its right.
    wm_w, wm_h = 320, 100
    for y in range(h - wm_h, h):
        for x in range(0, wm_w):
            px[x, y] = px[x + wm_w, y]
    print(f"watermark ({wm_w}x{wm_h} bottom-left) overwritten from the right")

    # 3) Iterative patches.
    for spec in args.patch:
        x, y, pw, ph = map(int, spec.split(","))
        rs = gs = bs = 0
        cnt = 0
        for yy in range(y, y + ph, 2):
            for xx in range(x, x + pw, 2):
                r, g, b = px[xx, yy]
                rs += r; gs += g; bs += b; cnt += 1
        d.rectangle([x, y, x + pw, y + ph], fill=(rs // cnt, gs // cnt, bs // cnt))
        print(f"patch {spec} -> local mean")
    for spec in args.patchsrc:
        x, y, pw, ph = map(int, spec.split(" ")[0].split(","))
        fx, fy = map(int, spec.split(" ")[1].split(","))
        for yy in range(y, y + ph):
            for xx in range(x, x + pw):
                px[xx, yy] = px[fx + xx - x, fy + yy - y]
        print(f"patchsrc {spec} -> copied")

    im.save(f"/tmp/{args.name}_clean.png")
    print(f"clean -> /tmp/{args.name}_clean.png")

    # Grid overlay for the manifest measurement.
    g = im.copy()
    dg = ImageDraw.Draw(g)
    for i in range(1, 20):
        x = w * i // 20
        dg.line([(x, 0), (x, h)], fill=(255, 0, 255), width=1)
        dg.text((x + 2, 5), str(i * 5), fill=(255, 0, 255))
        y = h * i // 20
        dg.line([(0, y), (w, y)], fill=(0, 255, 255), width=1)
        dg.text((5, y + 2), str(i * 5), fill=(0, 255, 255))
    g.resize((w // 2, h // 2), Image.LANCZOS).save(f"/tmp/{args.name}_grid.png")
    im.crop((0, 0, w, int(h * 0.25))).save(f"/tmp/{args.name}_top.png")
    print(f"grid -> /tmp/{args.name}_grid.png   top -> /tmp/{args.name}_top.png")


if __name__ == "__main__":
    main()
