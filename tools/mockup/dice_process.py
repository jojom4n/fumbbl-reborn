#!/usr/bin/env python3
"""
dice_process.py — turn the ComfyUI-rendered dice into clean client assets.

Reads the latest render of every face from ComfyUI's output dir (the
queue's filenames carry a take counter: block_2_00007_.png) and writes:

    assets/dice/block{1..6}.png   256x256, official-style block dice
    assets/dice/d6_{1..6}.png     256x256, red d6 with white pips
    assets/dice.qrc               Qt resource (prefix /dice) for the
                                  action log's rich-text <img> tags

Post-processing: center-crop to a square, drop any outer frame/margin,
resize to 256 with LANCZOS. The renders are already edge-to-edge per the
prompt, so the crop is a safety net, not a reframe.

Usage:
    python3 tools/mockup/dice_process.py [SRC_DIR]
    (default SRC_DIR = ComfyUI output/mockups/dice_set_v1)
"""

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent.parent
OUT = ROOT / "assets" / "dice"
SRC_DEFAULT = Path.home() / "ComfyUI-Shared/output/mockups/dice_set_v1"
SIZE = 256


def latest(src: Path, kind: str, value: int) -> Path:
    cands = sorted(src.glob(f"{kind}_{value}_*.png"))
    if not cands:
        raise SystemExit(f"no render for {kind}_{value} in {src}")
    # Highest take number wins (re-renders overwrite the take, not the file).
    def take(p: Path) -> int:
        try:
            return int(p.stem.split("_")[2])
        except (IndexError, ValueError):
            return 0
    return max(cands, key=take)


def to_asset(src_img: Path, dst: Path) -> None:
    im = Image.open(src_img).convert("RGB")
    # Center-crop to a square.
    w, h = im.size
    side = min(w, h)
    im = im.crop(((w - side) // 2, (h - side) // 2,
                  (w + side) // 2, (h + side) // 2))
    im = im.resize((SIZE, SIZE), Image.LANCZOS)
    im.save(dst)
    print(f"  {dst.name}  <-  {src_img.name}")


def write_qrc() -> None:
    lines = ['<RCC>', '    <qresource prefix="/dice">']
    for i in range(1, 7):
        lines.append(f'        <file alias="block{i}.png">dice/block{i}.png</file>')
    for i in range(1, 7):
        lines.append(f'        <file alias="d6_{i}.png">dice/d6_{i}.png</file>')
    lines += ["    </qresource>", "</RCC>"]
    (ROOT / "assets" / "dice.qrc").write_text("\n".join(lines) + "\n")
    print("  assets/dice.qrc")


def main() -> None:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else SRC_DEFAULT
    if not src.is_dir():
        raise SystemExit(f"src dir not found: {src}")
    OUT.mkdir(parents=True, exist_ok=True)
    print(f"source: {src}")
    for i in range(1, 7):
        to_asset(latest(src, "block", i), OUT / f"block{i}.png")
    for i in range(1, 7):
        to_asset(latest(src, "d6", i), OUT / f"d6_{i}.png")
    write_qrc()
    print("done")


if __name__ == "__main__":
    main()
