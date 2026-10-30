#!/usr/bin/env python3
"""Post-process the Qwen-rendered GW 2025 dugout symbols into client assets:
  1. keep only the LARGEST red connected component (drops the stray
     "10-12"/"8-9" text artifacts the model left under the symbol);
  2. transparent background (red-on-white chroma solve) so the dugout wood
     shows through between the ring and the symbol, like the official ones;
  3. centered 512x512 RGBA asset (4K-ready: the client downscales, never
     upscales, at any zoom level).

Outputs: assets/dugout/sym_reserve.png, sym_ko.png, sym_injured.png
"""
import os
import sys
from collections import deque
import numpy as np
from PIL import Image

SRC_DIR = os.path.expanduser("~/ComfyUI-Shared/output/mockups/symbols_v1")
OUT_DIR = "/home/saruman/Projects/FUMBBL-Reborn/assets/dugout"
os.makedirs(OUT_DIR, exist_ok=True)

PICKS = {
    "sym_reserve": "reserve_arrow_00001_.png",
    "sym_ko": "ko_face2_00002_.png",
    "sym_injured": "injured_skull_00001_.png",
}


def components(mask):
    """8-connected components; returns list of (size, component_mask)."""
    lab = np.zeros(mask.shape, int)
    cur = 0
    for y in range(mask.shape[0]):
        for x in range(mask.shape[1]):
            if mask[y, x] and lab[y, x] == 0:
                cur += 1
                q = deque([(y, x)])
                lab[y, x] = cur
                n = 0
                while q:
                    cy, cx = q.popleft()
                    n += 1
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if (0 <= ny < mask.shape[0] and 0 <= nx < mask.shape[1]
                                    and mask[ny, nx] and lab[ny, nx] == 0):
                                lab[ny, nx] = cur
                                q.append((ny, nx))
                yield n, lab == cur


def process(src, dst):
    im = Image.open(os.path.join(SRC_DIR, src)).convert("RGB")
    a = np.asarray(im).astype(float)
    # The symbol is the most saturated red in the image.
    sat = a.max(axis=2) - a.min(axis=2)
    ys, xs = np.where(sat > 100)
    if len(ys) == 0:
        print(src, "NO RED")
        return False
    sym = a[ys, xs].mean(axis=0)  # mean of the red pixels ~ symbol color
    # Alpha solve: out = a*sym + (1-a)*255  ->  a = (255-out)/(255-sym)
    denom = np.maximum(255.0 - sym, 1.0)
    alphas = (255.0 - a) / denom
    alpha = alphas.max(axis=2).clip(0, 1)
    # The symbol is MULTIPLE components (ring, inner symbol, crossbones, the
    # little stars). The artifacts are the small text the model wrote UNDER
    # the symbol. Keep every component that is (a) a decent size (>=5% of
    # the largest) and (b) NOT in the bottom band of the image.
    mask = alpha > 0.35
    H = mask.shape[0]
    keep = np.zeros_like(mask)
    coms = list(components(mask))
    if coms:
        biggest = max(c[0] for c in coms)
        for size, comp in coms:
            if size < max(60, 0.05 * biggest):
                continue
            ys3, xs3 = np.where(comp)
            cy = (ys3.min() + ys3.max()) / 2.0
            if cy > H * 0.86:  # the stray label text sits at the bottom
                continue
            keep |= comp
    alpha[~keep] = 0
    out = np.zeros((im.height, im.width, 4), np.uint8)
    # Color: un-premultiply toward the symbol color for clean transparency.
    af = alpha[..., None]
    color = np.where(af > 0.02, (a - (1.0 - af) * 255.0) / np.maximum(af, 0.02), sym)
    color = color.clip(0, 255).astype(np.uint8)
    out[..., :3] = color
    out[..., 3] = (alpha * 255).astype(np.uint8)
    # Center the content on a 512 canvas.
    ys2, xs2 = np.where(out[..., 3] > 0)
    crop = out[ys2.min():ys2.max() + 1, xs2.min():xs2.max() + 1]
    canvas = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    cw, ch = crop.shape[1], crop.shape[0]
    scale = min(512 * 0.96 / cw, 512 * 0.96 / ch)
    nw, nh = int(cw * scale), int(ch * scale)
    crop_im = Image.fromarray(crop).resize((nw, nh), Image.LANCZOS)
    canvas.paste(crop_im, ((512 - nw) // 2, (512 - nh) // 2), crop_im)
    canvas.save(dst)
    print("wrote", dst, "content", (cw, ch), "->", (nw, nh))
    return True


ok = True
for name, src in PICKS.items():
    ok &= process(src, os.path.join(OUT_DIR, name + ".png"))

# contact sheet
sheet = Image.new("RGB", (512 * 3 + 40, 542), (60, 50, 40))
for i, name in enumerate(PICKS):
    im = Image.open(os.path.join(OUT_DIR, name + ".png"))
    bg = Image.new("RGBA", (512, 512), (120, 90, 60, 255))
    bg.alpha_composite(im)
    sheet.paste(bg.convert("RGB"), (10 + i * 532, 10))
sheet.save("/tmp/sym_final.png")
print("sheet ->", "/tmp/sym_final.png")
sys.exit(0 if ok else 1)
