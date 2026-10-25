#!/usr/bin/env python3
"""Generator for the client's field asset (assets/field/field.png).

Bakes the Mockup-F field design into one PNG, cropped to the exact 26x15
field rect:
  * Qwen spectacular grass (skulls removed, the AI's inexact dotted grid
    erased, the AI's double center line erased)
  * TD zones rebuilt to ONE single column (col 0 / col 25, official):
    solid crimson + vertical TOUCHDOWN text
  * mowing stripes perfectly aligned to the grid (light/dark columns)

The Qt client draws on top at runtime: the exact 26x15 grid lines, the
side-zone boundaries, the single center line, team-logo watermarks,
tokens, ball, action wheel, target brackets and the rain overlay.

Re-run after re-rendering the Qwen field:
    python3 tools/mockup/make_field_asset.py [71|72]
"""
import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))

from PIL import Image  # noqa: E402

from compose_f import (CONFIGS, clean_grass, erase_center_line,  # noqa: E402
                       grid_mow_tint, narrow_td_zone, remove_skulls)


def build(key="71"):
    cfg = CONFIGS[key]
    im = Image.open(cfg["in"]).convert("RGB")
    im = remove_skulls(im, cfg.get("skulls", []))
    fx, fy, fw, fh = cfg["field"]
    zone = (cfg["banner_x"][0] + 4, cfg["banner_x"][1] - 4, fy, fy + fh)
    im = clean_grass(im, zone)
    im, _ = erase_center_line(im, cfg["field"])
    im = narrow_td_zone(im, cfg["field"], cfg["banner_x"])
    im = grid_mow_tint(im, cfg["field"])
    # crop to the exact 26x15 field rect (cells become near-square)
    out = im.crop((fx, fy, fx + fw, fy + fh))
    dest = HERE.parent.parent / "assets" / "field" / "field.png"
    dest.parent.mkdir(parents=True, exist_ok=True)
    out.save(dest, "PNG")
    print("saved", dest, out.size)


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "71")
