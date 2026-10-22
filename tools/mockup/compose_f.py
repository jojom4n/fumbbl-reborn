#!/usr/bin/env python3
"""Mockup F compositor — light-style UI over the Qwen spectacular field.

Pipeline (mockup only, NO Qt implementation involved):
  1. Qwen-Image 2.1 renders the FIELD ALONE (grass, mowing stripes, rain
     sheen, thick center line, crimson TOUCHDOWN zones with skulls) — see
     f_field_queue.py.
  2. Grid cleanup: the AI's faint dotted grid is NOT exactly 26x15, so it is
     erased in the grass area (median pass + procedural grass-blade overlay
     to restore micro-texture) and a CRISP EXACT 26x15 grid is redrawn,
     aligned to the banners, with the center line at the true midpoint.
  3. REAL FUMBBL data is composited (match 1955576, live-dumped via
     spectator_smoke):
       * REAL team logos (fumbbl.com /i/<id>) at ~13% alpha, centered on
         each half. THE LOGO SYSTEM: per-match, data-driven (the match API
         returns `logo`/`logolarge` per team); the Qt client will do the
         same from the server's team logoUrl.
       * real player tokens at their real coordinates (faces, team rings,
         numbers, ball, prone/stunned states, acting ring)
       * the light-style chrome (final spec 22/ott: white/light-gray
         translucent strips + soft shadow, team accent colors, red reserved
         for injury/failure):
           - top header (BB2 sides + BB3 center: score, turn, END OF TURN
             timer, weather icon)
           - ultra-thin vertical box strips on the left/right edges
             (reserve / KO / injured slots)
           - bottom-center ticker (BB2 style) with expanded history showing
             REAL dice faces (pips drawn)
           - chat BUTTON with unread badge (chat is overlay-on-demand)
           - translucent side player card (real stats) on the side opposite
             the acting player
           - dark radial wheel + BB2-style target brackets on the acting
             player
       * subtle rain streaks (live match weather: Pouring Rain)

Usage: python3 compose_f.py [71|72|all]
"""
import json
import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).parent
REPO = HERE.parent.parent
FACES = REPO / "assets" / "faces"
LOGOS = Path("/tmp/mockF/logos")
LIVE = Path("/tmp/mockF/live2.json")
OUTDIR = REPO / "faces-work" / "mockups"
OUTDIR.mkdir(parents=True, exist_ok=True)

W, H = 1344, 768

# ----------------------------------------------------------------- palette
# Final spec: NO dark fantasy. White/light gray + team accents + red only
# for injury/failure.
INK = (34, 40, 50)          # primary text
INK2 = (104, 114, 128)      # secondary text
PAPER = (250, 250, 253)     # strip base
HOME = (52, 108, 180)       # Pearls before a Dog (Old World Alliance) accent
AWAY = (193, 152, 55)       # Wyldyr Heartseekers (Wood Elf) accent
RED = (196, 60, 52)         # injury / failure ONLY
GREEN = (60, 142, 84)       # success
GOLD = (212, 175, 55)

_FONT_CACHE = {}
def font(size, weight="reg"):
    key = (size, weight)
    if key not in _FONT_CACHE:
        name = {"reg": "NotoSans-Regular", "med": "NotoSans-Medium",
                "bold": "NotoSans-Bold", "black": "NotoSans-Black"}[weight]
        _FONT_CACHE[key] = ImageFont.truetype(f"/usr/share/fonts/noto/{name}.ttf", size)
    return _FONT_CACHE[key]

def text_w(d, s, fnt):
    b = d.textbbox((0, 0), s, font=fnt)
    return b[2] - b[0]

def center_text(d, cx, y, s, fnt, fill):
    d.text((cx - text_w(d, s, fnt) / 2, y), s, font=fnt, fill=fill)

# =================================================================== field
def grass_overlay(w, h, seed=3, density=10):
    """Procedural grass-blade micro-texture (grain + vertical strokes)."""
    rng = random.Random(seed)
    grain = np.random.RandomState(seed).randint(0, 255, (h, w), dtype=np.uint8)
    ov = Image.fromarray(grain, "L")
    d = ImageDraw.Draw(ov)
    for _ in range(w * h // density):
        x = rng.randrange(w)
        y = rng.randrange(h)
        L = rng.randint(5, 20)
        v = rng.choice((150, 160, 170, 90, 100, 110))
        d.line((x, y, x, y + L), fill=v, width=1)
    return ov

def zone_mask(h, w, zone, feather=8):
    m = np.ones((h, w), float)
    m[:, :zone[0]] = 0
    m[:, zone[1]:] = 0
    m[:zone[2], :] = 0
    m[zone[3]:, :] = 0
    mi = Image.fromarray((m * 255).astype(np.uint8)).filter(
        ImageFilter.GaussianBlur(feather))
    return np.asarray(mi).astype(float) / 255

def clean_grass(im, zone, med=9, strength=0.55, seed=3):
    """Erase the AI's faint dotted grid (median kills 1-2px dots, keeps the
    mowing stripes) and restore grass micro-texture (blade overlay)."""
    im = im.convert("RGB")
    w, h = im.size
    medim = im.filter(ImageFilter.MedianFilter(med))
    a = np.asarray(im).astype(float)
    b = np.asarray(medim).astype(float)
    m = zone_mask(h, w, zone)
    a = a * (1 - m[..., None]) + b * m[..., None]
    ov = grass_overlay(w, h, seed)
    ovb = 1 + (np.asarray(ov).astype(float) / 255.0 - 0.5) * strength
    out = a * ovb[..., None]
    res = a * (1 - m[..., None]) + np.clip(out, 0, 255) * m[..., None]
    return Image.fromarray(np.clip(res, 0, 255).astype(np.uint8))

def erase_center_line(im, field, halfwin=110):
    """Find the AI's thick center line (brightest column near the middle)
    and replace its band with local grass from both sides."""
    x0, y0, fw, fh = field
    a = np.asarray(im).astype(float)
    band = a[y0:y0+fh, x0:x0+fw, :].sum(axis=2)
    cx_local = int(np.argmax(band.max(axis=0)[fw//2-halfwin:fw//2+halfwin])
                   + fw//2 - halfwin)
    cx = x0 + cx_local
    h = a.shape[0]
    for dx in range(-9, 10):
        x = cx + dx
        if 0 <= x < a.shape[1]:
            lo = max(0, cx - 40)
            hi = max(1, cx - 28)
            lo2 = min(a.shape[1]-1, cx + 28)
            hi2 = min(a.shape[1], cx + 40)
            left = a[y0:y0+fh, lo:hi, :].mean(axis=1)
            right = a[y0:y0+fh, lo2:hi2, :].mean(axis=1)
            t = (abs(dx) - 1.5) / 7.5
            t = max(0.0, min(1.0, t))
            a[y0:y0+fh, x, :] = a[y0:y0+fh, x, :] * (1 - t) + (left + right) / 2 * t
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)), cx

def draw_exact_grid(im, field, banner_x):
    """Crisp exact 26x15 grid: vertical inner lines between the banners,
    horizontal lines across the same span, thick center line at the true
    midpoint."""
    x0, y0, fw, fh = field
    cw, ch = fw / 26, fh / 15
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    soft = (255, 255, 255, 110)
    # vertical lines (skip the ones under the banners)
    for i in range(1, 26):
        x = x0 + i * cw
        if banner_x[0] < x < banner_x[1]:
            d.line((x, y0, x, y0 + fh), fill=soft, width=2)
    # horizontal lines
    for j in range(0, 16):
        y = y0 + j * ch
        d.line((banner_x[0], y, banner_x[1], y), fill=soft, width=2)
    # center line (thick, crisp)
    cx = x0 + 13 * cw
    d.line((cx, y0, cx, y0 + fh), fill=(255, 255, 255, 235), width=7)
    im.paste(Image.alpha_composite(im.convert("RGBA"), layer).convert("RGB"))
    return cw, ch

# =================================================================== data
def load_live():
    d = json.load(open(LIVE))
    g = d["game"]
    th, ta = g["teamHome"], g["teamAway"]
    hmap = {p["playerId"]: p for p in th["playerArray"]}
    amap = {p["playerId"]: p for p in ta["playerArray"]}
    players = []
    for p in g["fieldModel"]["playerDataArray"]:
        x, y = p["playerCoordinate"]
        if x < 0:  # in the box (no field coordinate)
            continue
        src = hmap.get(p["playerId"]) or amap.get(p["playerId"])
        players.append({
            "side": "H" if p["playerId"] in hmap else "A",
            "name": src["playerName"], "nr": src["playerNr"],
            "x": x, "y": y, "state": p["playerState"],
            "posId": src["positionId"],
        })
    boxes = {"H": [], "A": []}
    for side, team, hmap_ in (("H", th, hmap), ("A", ta, amap)):
        for p in team["playerArray"]:
            st = next(q["playerState"] for q in g["fieldModel"]["playerDataArray"]
                      if q["playerId"] == p["playerId"])
            if st & 0xFF in (9, 11, 12, 0xA):
                boxes[side].append((p["playerName"], st & 0xFF, p))
    return {
        "home": th, "away": ta, "players": players, "boxes": boxes,
        "ball": g["fieldModel"]["ballCoordinate"],
        "ball_in_play": g["fieldModel"]["ballInPlay"],
        "weather": g["fieldModel"].get("weather"),
        "half": g["half"],
        "turn": g["turnDataHome"]["turnNr"],
        "home_playing": g["homePlaying"],
        "turn_time": g["turnTime"],
        "turn_limit": 240,  # gameOptions.turntime (seconds)
    }

# face slug per player (position-based; OWA = mixed human/dwarf, Wood Elf = elves)
def home_face(name):
    n = name.lower()
    if "halfling" in n: return "halfling_hopeful"
    if "d blitzer" in n: return "dwarf_blitzer"
    if "d slayer" in n: return "troll_slayer"
    if "ogre" in n: return "ogre"
    if "d lino" in n: return "dwarf_lineman"
    if "h blitzer" in n: return "human_blitzer"
    if "h catcher" in n: return "human_catcher"
    if "h thrower" in n: return "human_thrower"
    return "human_lineman"

AWAY_POS = {  # positionId -> slug (from the team roster)
    66176: "wood_elf_lineman", 66177: "wood_elf_thrower",
    66178: "wood_elf_catcher", 66179: "elf_blitzer",
    66180: "loren_forest_treeman",
}

def away_face(p):
    return AWAY_POS.get(p["posId"], "wood_elf_lineman")

ACTING_NAME = "H Thrower Gerirr"  # the player the mockup spotlights

# =================================================================== parts
def token_image(slug, size, ring, prone=False):
    img = Image.new("RGBA", (int(size), int(size)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    dark = tuple(max(0, c - 45) for c in ring)
    cx = size / 2
    if prone:
        w, h = size, size * 0.56
        d.ellipse((cx - w / 2, size / 2 - h / 2, cx + w / 2, size / 2 + h / 2),
                  fill=ring + (235,), outline=dark + (235,), width=2)
    else:
        d.ellipse((cx - size / 2, cx - size / 2, cx + size / 2, cx + size / 2),
                  fill=ring + (235,), outline=dark + (235,), width=2)
    face = Image.open(FACES / f"{slug}.png").convert("RGBA")
    fs = size * 0.8
    if prone:
        face = face.resize((int(fs * 1.16), int(fs * 0.55)), Image.LANCZOS)
    else:
        face = face.resize((int(fs), int(fs)), Image.LANCZOS)
    mask = Image.new("L", face.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, face.size[0], face.size[1]), fill=255)
    if prone:
        img.paste(face, (int(cx - face.width / 2), int(size / 2 - face.height / 2)), mask)
    else:
        img.paste(face, (int(cx - fs / 2), int(cx - fs / 2)), mask)
    return img

def number_badge(size, nr):
    img = Image.new("RGBA", (int(size), int(size)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((0, 0, size - 1, size - 1), fill=(24, 26, 32, 235),
              outline=(216, 180, 74, 235), width=1)
    f = font(max(8, int(size * 0.55)), "bold")
    center_text(d, size / 2, size / 2 - text_w(d, str(nr), f) / 2 - 1,
                str(nr), f, (255, 255, 255, 255))
    return img

def block_dice(value, size=26):
    """GW-official-style block die face (crisp vector redraw): dark rounded
    die + white symbol.  Values (ffb BlockDiceCategory):
      1 skull (player down)   2 skull+star (both down)
      3 arrow (push 1)        4 double arrow (push 2)
      5 star+arrow (stumble)  6 star (POW)"""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((1, 1, size - 2, size - 2), radius=size // 5,
                        fill=(32, 34, 40, 255), outline=(255, 255, 255, 200), width=1)
    c = size / 2
    WT = (255, 255, 255, 255)

    def star(cx, cy, r, n=5):
        pts = []
        for i in range(n * 2):
            rr = r if i % 2 == 0 else r * 0.46
            ang = -math.pi / 2 + i * math.pi / n
            pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
        return pts

    def skull(cx, cy, r):
        d.ellipse((cx - r * 0.9, cy - r * 0.95, cx + r * 0.9, cy + r * 0.5), fill=WT)
        d.rectangle((cx - r * 0.5, cy + r * 0.25, cx + r * 0.5, cy + r * 0.9), fill=WT)
        dc = (32, 34, 40, 255)
        d.ellipse((cx - r * 0.62, cy - r * 0.55, cx - r * 0.1, cy + r * 0.02), fill=dc)
        d.ellipse((cx + r * 0.1, cy - r * 0.55, cx + r * 0.62, cy + r * 0.02), fill=dc)
        for tx in (-0.3, 0.0, 0.3):
            d.line((cx + tx * r, cy + r * 0.42, cx + tx * r, cy + r * 0.82),
                   fill=dc, width=1)

    def arrow(cx, cy, r, thin=False):
        wgt = max(1, int(r * 0.34)) if thin else max(2, int(r * 0.46))
        d.line((cx, cy - r, cx, cy + r * 0.25), fill=WT, width=wgt)
        d.polygon([(cx - wgt * 1.5, cy), (cx + wgt * 1.5, cy), (cx, cy + r)],
                  fill=WT)

    if value == 1:
        skull(c, c, size * 0.21)
    elif value == 2:
        skull(c + size * 0.07, c + size * 0.10, size * 0.165)
        d.polygon(star(c - size * 0.19, c - size * 0.19, size * 0.115), fill=WT)
    elif value == 3:
        arrow(c, c, size * 0.26)
    elif value == 4:
        arrow(c - size * 0.14, c, size * 0.22, thin=True)
        arrow(c + size * 0.14, c, size * 0.22, thin=True)
    elif value == 5:
        d.polygon(star(c, c - size * 0.14, size * 0.125), fill=WT)
        arrow(c, c + size * 0.17, size * 0.175)
    elif value == 6:
        d.polygon(star(c, c, size * 0.27), fill=WT)
    return img

def reroll_icon(d, cx, cy, r=6.5):
    """Dedicated small reroll glyph: circular arrow."""
    d.arc((cx - r, cy - r, cx + r, cy + r), start=290, end=200,
          fill=INK2 + (235,), width=2)
    ax = cx + r * math.cos(math.radians(200))
    ay = cy + r * math.sin(math.radians(200))
    d.polygon([(ax - 1.5, ay - 4.5), (ax + 4.0, ay - 1.5), (ax - 2.5, ay + 2.0)],
              fill=INK2 + (235,))

def remove_skulls(im, boxes):
    """Inpaint TD-banner skulls by texture transfer: copy a clean banner
    patch from just above each skull (same x range), jittered + slight noise."""
    a = np.asarray(im.convert("RGB")).astype(np.int16)
    rng = np.random.RandomState(11)
    for (x0, y0, x1, y1) in boxes:
        x0, y0, x1, y1 = x0 - 3, y0 - 3, x1 + 4, y1 + 4
        h, w = y1 - y0, x1 - x0
        src_y0 = max(0, y0 - h - 6)
        src_y1 = src_y0 + h
        if src_y1 <= src_y0:
            continue
        patch = a[src_y0:src_y1, x0:x1, :].copy()
        patch = np.roll(patch, int(rng.randint(2, 6)), axis=0)
        noise = rng.randint(-5, 6, (h, w, 3)).astype(np.int16)
        a[y0:y1, x0:x1, :] = patch + noise
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

def rain_icon(d, x, y, s=20):
    d.ellipse((x, y + s * 0.25, x + s * 0.75, y + s * 0.85), fill=(168, 180, 196, 255))
    d.ellipse((x + s * 0.25, y, x + s * 0.95, y + s * 0.6), fill=(168, 180, 196, 255))
    d.ellipse((x + s * 0.55, y + s * 0.25, x + s, y + s * 0.8), fill=(168, 180, 196, 255))
    for dx in (0.2, 0.5, 0.8):
        x0 = x + s * dx
        d.line((x0, y + s * 0.9, x0 - s * 0.08, y + s * 1.3),
               fill=(90, 140, 200, 255), width=2)

def draw_wheel(canvas, cx, cy, cell):
    """White circular action wheel around the acting player: clean disc,
    dark icons, tight ring (standard game-UI pattern)."""
    R = cell * 1.28
    sh = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse((cx - R + 2, cy - R + 5, cx + R + 2, cy + R + 5),
                               fill=(20, 26, 34, 70))
    sh = sh.filter(ImageFilter.GaussianBlur(5))
    canvas.alpha_composite(sh)
    disc = Image.new("RGBA", (int(R * 2) + 4, int(R * 2) + 4), (0, 0, 0, 0))
    dd = ImageDraw.Draw(disc)
    dd.ellipse((2, 2, int(R * 2) + 2, int(R * 2) + 2),
               fill=(252, 252, 254, 218), outline=(168, 176, 190, 255), width=2)
    canvas.alpha_composite(disc, (int(cx - R - 2), int(cy - R - 2)))
    d = ImageDraw.Draw(canvas)
    INKc = INK + (255,)
    ic = cell * 0.30
    rr = R * 0.60
    # move (up arrow)
    d.line((cx, cy - rr - ic * 0.5, cx, cy - rr + ic * 0.35), fill=INKc, width=3)
    d.polygon([(cx - ic * 0.42, cy - rr + ic * 0.05), (cx + ic * 0.42, cy - rr + ic * 0.05),
               (cx, cy - rr - ic * 0.55)], fill=INKc)
    # block (two shields)
    bx, by = cx + rr * 0.87, cy - rr * 0.5
    for off in (-ic * 0.42, ic * 0.14):
        d.rounded_rectangle((bx + off - ic * 0.18, by - ic * 0.3,
                             bx + off + ic * 0.18, by + ic * 0.3), radius=2, fill=INKc)
    # blitz (zigzag)
    zx, zy = cx + rr * 0.87, cy + rr * 0.5
    d.line((zx - ic * 0.45, zy - ic * 0.35, zx - ic * 0.05, zy,
            zx - ic * 0.35, zy + ic * 0.35, zx + ic * 0.4, zy - ic * 0.1),
           fill=INKc, width=3)
    # pass (arc + arrowhead)
    px, py = cx - rr * 0.87, cy + rr * 0.5
    d.arc((px - ic * 0.5, py - ic * 0.5, px + ic * 0.5, py + ic * 0.5),
          180, 20, fill=INKc, width=3)
    d.polygon([(px + ic * 0.32, py - ic * 0.42), (px + ic * 0.62, py - ic * 0.18),
               (px + ic * 0.18, py - ic * 0.05)], fill=INKc)
    # foul (crossed swords)
    fx, fy = cx - rr * 0.87, cy - rr * 0.5
    d.line((fx - ic * 0.4, fy - ic * 0.4, fx + ic * 0.4, fy + ic * 0.4),
           fill=INKc, width=3)
    d.line((fx + ic * 0.4, fy - ic * 0.4, fx - ic * 0.4, fy + ic * 0.4),
           fill=INKc, width=3)
    # attack (dagger)
    ax, ay = cx, cy + rr
    d.polygon([(ax, ay + ic * 0.5), (ax - ic * 0.28, ay - ic * 0.3),
               (ax, ay - ic * 0.55), (ax + ic * 0.28, ay - ic * 0.3)], fill=INKc)
    d.line((ax - ic * 0.4, ay - ic * 0.35, ax + ic * 0.4, ay - ic * 0.35),
           fill=INKc, width=2)

def target_brackets(d, rect, color=(255, 255, 255, 215)):
    x0, y0, x1, y1 = rect
    a, w = 8, 2
    for cx, cy, sx, sy in ((x0+1, y0+1, 1, 1), (x1-1, y0+1, -1, 1),
                           (x0+1, y1-1, 1, -1), (x1-1, y1-1, -1, -1)):
        d.line((cx, cy, cx + sx * a, cy), fill=color, width=w)
        d.line((cx, cy, cx, cy + sy * a), fill=color, width=w)

def team_logo(path, size, alpha=0.13):
    img = Image.open(path).convert("RGBA")
    img = img.resize((size, size), Image.LANCZOS)
    img.putalpha(img.getchannel("A").point(lambda v: int(v * alpha)))
    return img

# =================================================================== scene
# field = exact 26x15 grid rect (x, y, w, h); banner_x = inner edges of the
# two TOUCHDOWN banners (the AI's wide banner bands cover ~2 columns each).
CONFIGS = {
    "71": {"in": "/home/saruman/ComfyUI-Shared/output/mockups/F_field_s71_00001_.png",
           "field": (31, 0, 1278, 739), "banner_x": (136, 1206),
           "out": "FINAL_F_71.png",
           # AI-painted TD skull bboxes (removed via texture transfer)
           "skulls": [(55, 129, 106, 187), (1234, 129, 1286, 187)]},
    "72": {"in": "/home/saruman/ComfyUI-Shared/output/mockups/F_field_s72_00001_.png",
           "field": (51, 23, 1244, 705), "banner_x": (131, 1212),
           "out": "FINAL_F_72.png",
           "skulls": [(64, 190, 108, 243), (1231, 190, 1275, 243)]},
}

def compose(key):
    cfg = CONFIGS[key]
    live = load_live()
    th, ta = live["home"], live["away"]

    # --- 1. field: clean the AI grid, draw the exact 26x15
    field = Image.open(cfg["in"]).convert("RGB")
    field = remove_skulls(field, cfg.get("skulls", []))
    fx, fy, fw, fh = cfg["field"]
    zone = (cfg["banner_x"][0] + 4, cfg["banner_x"][1] - 4, fy, fy + fh)
    field = clean_grass(field, zone)
    field, _ = erase_center_line(field, cfg["field"])
    cw, ch = draw_exact_grid(field, cfg["field"], cfg["banner_x"])
    canvas = field.convert("RGBA")

    def cell_rect(x, y):
        return (fx + x * cw, fy + y * ch, fx + (x + 1) * cw, fy + (y + 1) * ch)
    def cell_center(x, y):
        return (fx + (x + 0.5) * cw, fy + (y + 0.5) * ch)

    # --- 2. TEAM LOGOS: per-match real logos, ~13% alpha, center of each half
    logo_sz = int(fh * 0.52)
    for path, col in ((LOGOS / "637106.png", 6.5), (LOGOS / "486361.png", 19.5)):
        cx = fx + (col) * cw
        cy = fy + fh / 2
        lg = team_logo(path, logo_sz, alpha=0.10)
        canvas.paste(lg, (int(cx - logo_sz / 2), int(cy - logo_sz / 2)), lg)

    d = ImageDraw.Draw(canvas)

    # --- 3. target brackets (BB2 style) on empty cells ahead of the thrower
    for (tx, ty) in ((12, 8), (13, 8)):
        target_brackets(d, cell_rect(tx, ty))

    # --- 4. white action wheel on the acting player (under the token)
    act_p = next(p for p in live["players"] if p["name"] == ACTING_NAME)
    ax, ay = cell_center(act_p["x"], act_p["y"])
    draw_wheel(canvas, ax, ay, min(cw, ch))
    d = ImageDraw.Draw(canvas)

    # --- 5. tokens (real coordinates, real faces, real states)
    ball = live["ball"]
    for p in live["players"]:
        ring = HOME if p["side"] == "H" else AWAY
        slug = home_face(p["name"]) if p["side"] == "H" else away_face(p)
        st = p["state"] & 0xFF
        acting = (p["name"] == ACTING_NAME)
        prone = (st == 3)
        size = min(cw, ch) * 0.95
        cx, cy = cell_center(p["x"], p["y"])
        tok = token_image(slug, size, ring, prone=prone)
        canvas.paste(tok, (int(cx - size / 2), int(cy - size / 2)), tok)
        bd = number_badge(size * 0.42, p["nr"])
        canvas.paste(bd, (int(cx + size * 0.28), int(cy + size * 0.24)), bd)
        if st == 4:  # stunned: stars
            for i in range(3):
                sx = cx - 12 + i * 12
                sy = cy - size * 0.72
                d.regular_polygon((sx, sy, 4), 5, rotation=0,
                                  fill=(255, 220, 90, 255), outline=(120, 90, 20, 255))
        # acting player is indicated by the white wheel (drawn below)
    # loose ball (drawn peeking from under the token standing on the square)
    if live["ball_in_play"] and ball:
        ox, oy = cell_center(ball[0], ball[1])
        d.ellipse((ox + cw * 0.10, oy + ch * 0.22, ox + cw * 0.10 + cw * 0.44,
                   oy + ch * 0.22 + ch * 0.32),
                  fill=(139, 90, 43, 255), outline=(64, 38, 18, 255), width=1)

    # --- 6. rain (MicroProse-Soccer style: fine top-down droplet specks)
    rng = random.Random(7)
    for _ in range(520):
        x0 = rng.uniform(0, W)
        y0 = rng.uniform(0, H)
        a = rng.randint(35, 115)
        r = rng.choice((0.9, 0.9, 1.1, 1.5))
        d.ellipse((x0, y0, x0 + r, y0 + r), fill=(235, 240, 250, a))
    for _ in range(90):
        x0 = rng.uniform(0, W)
        y0 = rng.uniform(0, H)
        L = rng.uniform(2.5, 6.0)
        a = rng.randint(28, 72)
        d.line((x0, y0, x0, y0 + L), fill=(235, 240, 250, a), width=1)

    # --- 7. chrome (light strips + shadow)
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shd = ImageDraw.Draw(shadow)
    chrome = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(chrome)

    def shadow_box(box, r=10, off=(0, 3)):
        x0, y0, x1, y1 = box
        shd.rounded_rectangle((x0, y0 + off[1], x1, y1 + off[1]),
                              radius=r, fill=(0, 0, 0, 70))

    # header
    HDR = (0, 0, W, 62)
    shadow_box(HDR, r=0)
    d.rectangle(HDR, fill=PAPER + (225,))
    d.line((0, 61, W, 61), fill=(0, 0, 0, 40), width=1)

    crest_h = Image.open(LOGOS / "637109.png").convert("RGBA").resize((40, 40), Image.LANCZOS)
    crest_a = Image.open(LOGOS / "486359.png").convert("RGBA").resize((40, 40), Image.LANCZOS)
    chrome.paste(crest_h, (14, 11), crest_h)
    chrome.paste(crest_a, (W - 54, 11), crest_a)
    d = ImageDraw.Draw(chrome)

    nm_h = th["teamName"].upper()
    nm_a = ta["teamName"].upper()
    d.text((64, 8), nm_h, font=font(14, "bold"), fill=INK + (255,))
    d.text((64, 28), f"{th['coach']}  ·  {th['race']}", font=font(10), fill=INK2 + (255,))
    d.text((W - 64 - text_w(d, nm_a, font(14, "bold")), 8), nm_a,
           font=font(14, "bold"), fill=INK + (255,))
    sub_a = f"{ta['coach']}  ·  {ta['race']}"
    d.text((W - 64 - text_w(d, sub_a, font(10)), 28), sub_a, font=font(10),
           fill=INK2 + (255,))

    n_rh, n_ra = th["reRolls"], ta["reRolls"]
    sub_h = f"{th['coach']}  ·  {th['race']}"
    # END TURN button next to the active human coach (home is playing)
    if live["home_playing"]:
        ex = 64 + text_w(d, sub_h, font(10)) + 14
        d.rounded_rectangle((ex, 24, ex + 82, 46), radius=11, fill=HOME + (255,),
                            outline=tuple(max(0, c - 60) for c in HOME) + (255,),
                            width=1)
        d.text((ex + 8, 29), "END TURN", font=font(10, "bold"),
               fill=(255, 255, 255, 255))
        rr_x = ex + 94
    else:
        rr_x = 64 + text_w(d, sub_h, font(10)) + 14
    reroll_icon(d, rr_x + 7, 35)
    d.text((rr_x + 18, 27), str(n_rh), font=font(12, "bold"), fill=INK + (255,))
    # away: rerolls just left of the coach text
    rr_x2 = W - 64 - text_w(d, sub_a, font(10)) - 44
    reroll_icon(d, rr_x2 + 7, 35)
    d.text((rr_x2 + 18, 27), str(n_ra), font=font(12, "bold"), fill=INK + (255,))

    # center: score + turn indicator + chips + timer + weather
    score = "0 : 0"
    fs = font(22, "black")
    scw = text_w(d, score, fs)
    d.text((W / 2 - scw / 2, 6), score, font=fs, fill=INK + (255,))
    dotc = HOME if live["home_playing"] else AWAY
    if live["home_playing"]:
        d.ellipse((W / 2 - scw / 2 - 15, 13, W / 2 - scw / 2 - 5, 23), fill=dotc + (255,))
    else:
        d.ellipse((W / 2 + scw / 2 + 5, 13, W / 2 + scw / 2 + 15, 23), fill=dotc + (255,))

    ty = 36
    def chip(s, col, x):
        d.rounded_rectangle((x, ty, x + text_w(d, s, font(10, "bold")) + 14, ty + 16),
                            radius=8, outline=col + (200,), width=1)
        d.text((x + 7, ty + 2), s, font=font(10, "bold"), fill=col + (255,))
        return x + text_w(d, s, font(10, "bold")) + 14
    cx0 = W / 2 - 132
    nx = chip(f"TURN {live['turn']}", HOME, cx0) + 8
    nx = chip(f"{live['half']}ST HALF", INK2, nx)
    remain = max(0, live["turn_limit"] - live["turn_time"] / 1000)
    frac = remain / live["turn_limit"]
    bx0, by0 = nx + 12, ty + 2
    d.rounded_rectangle((bx0, by0, bx0 + 110, by0 + 12), radius=6,
                        fill=(226, 228, 234, 255))
    if frac > 0:
        d.rounded_rectangle((bx0, by0, bx0 + max(12, 110 * frac), by0 + 12),
                            radius=6, fill=GREEN + (230,))
    t = f"{int(remain // 60)}:{int(remain % 60):02d}"
    d.text((bx0 + 118, ty), t, font=font(11, "bold"), fill=INK + (255,))
    rain_icon(d, bx0 + 118 + text_w(d, t, font(11, "bold")) + 12, ty - 4, s=18)

    # box strips (ultra-thin vertical, left = home, right = away)
    def box_strip(x, team, color, box_players):
        bx = (x, 120, x + 46, 430)
        shadow_box(bx, r=12)
        d.rounded_rectangle(bx, radius=12, fill=PAPER + (205,))
        d.rounded_rectangle((x + 4, 124, x + 42, 130), radius=3, fill=color + (220,))
        y = 140
        for label, code in (("RESERVE", 9), ("KO", 11), ("INJURED", 12)):
            d.text((x + 8, y), label, font=font(7, "bold"), fill=INK2 + (220,))
            y += 13
            mine = [b for b in box_players if b[1] == code]
            n = max(2, len(mine))
            for i in range(n):
                sy = y + i * 36 + 16
                if i < len(mine):
                    nm, _, _ = mine[i]
                    slug = (home_face(nm) if team == "H" else "wood_elf_lineman")
                    face = Image.open(FACES / f"{slug}.png").convert("RGBA")
                    face = face.resize((26, 26), Image.LANCZOS)
                    mask = Image.new("L", face.size, 0)
                    ImageDraw.Draw(mask).ellipse((0, 0, 26, 26), fill=255)
                    chrome.paste(face, (x + 23 - 13, sy - 13), mask)
                    d.ellipse((x + 8, sy - 15, x + 38, sy + 15),
                              outline=color + (230,), width=2)
                else:
                    d.ellipse((x + 8, sy - 15, x + 38, sy + 15),
                              outline=(160, 166, 178, 160), width=2)
            y += n * 36 + 8

    box_strip(8, "H", HOME, live["boxes"]["H"])
    box_strip(W - 54, "A", AWAY, live["boxes"]["A"])

    # player card (acting thrower; side opposite the player -> right edge)
    p = next(q for q in th["playerArray"] if "Gerirr" in q["playerName"])
    CW_, CH_ = 224, 316
    CX0, CY0 = W - 54 - CW_ - 12, 96
    card = (CX0, CY0, CX0 + CW_, CY0 + CH_)
    shadow_box(card, r=14)
    d.rounded_rectangle(card, radius=14, fill=PAPER + (232,))
    d.rounded_rectangle(card, radius=14, outline=HOME + (160,), width=1)
    d.rectangle((CX0 + 4, CY0 + 12, CX0 + 8, CY0 + CH_ - 12), fill=HOME + (230,))
    cxm = CX0 + CW_ // 2 + 2
    fsz = 88
    face = Image.open(FACES / "human_thrower.png").convert("RGBA").resize((fsz, fsz), Image.LANCZOS)
    mask = Image.new("L", face.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, fsz, fsz), fill=255)
    chrome.paste(face, (cxm - fsz // 2, CY0 + 14), mask)
    d = ImageDraw.Draw(chrome)
    d.ellipse((cxm - fsz // 2, CY0 + 14, cxm + fsz // 2, CY0 + 14 + fsz),
              outline=HOME + (220,), width=2)
    center_text(d, cxm, CY0 + 108, p["playerName"].upper(), font(16, "bold"), INK + (255,))
    center_text(d, cxm, CY0 + 128, f"#{p['playerNr']}  ·  H Thrower", font(10), INK2 + (255,))
    center_text(d, cxm, CY0 + 142, th["teamName"], font(9), INK2 + (235,))
    d.line((CX0 + 16, CY0 + 160, CX0 + CW_ - 16, CY0 + 160), fill=(0, 0, 0, 40), width=1)
    stats = [("MA", p["movement"]), ("ST", p["strength"]), ("AG", p["agility"]),
             ("PA", p["armour"]), ("AV", p["armour"] + p["strength"])]
    colw = (CW_ - 24) // 5
    for i, (lb, v) in enumerate(stats):
        px = CX0 + 12 + i * colw + colw // 2
        center_text(d, px, CY0 + 168, lb, font(9, "bold"), INK2 + (235,))
        center_text(d, px, CY0 + 182, str(v), font(16, "black"), INK + (255,))
    sy = CY0 + 212
    center_text(d, cxm, sy, "SKILLS", font(8, "bold"), INK2 + (235,))
    sx = CX0 + 14
    for sk in p["skillArray"][:3]:
        lbl = sk.upper()
        tw_ = text_w(d, lbl, font(9, "bold"))
        d.rounded_rectangle((sx, sy + 14, sx + tw_ + 14, sy + 34), radius=10,
                            fill=(238, 240, 246, 255), outline=HOME + (150,), width=1)
        d.text((sx + 7, sy + 16), lbl, font=font(9, "bold"), fill=INK + (245,))
        sx += tw_ + 22
    center_text(d, cxm, CY0 + CH_ - 52, "STANDING", font(11, "bold"), GREEN + (255,))
    d.rounded_rectangle((cxm - 34, CY0 + CH_ - 40, cxm + 34, CY0 + CH_ - 22),
                        radius=9, fill=HOME + (215,))
    center_text(d, cxm, CY0 + CH_ - 38, "ACTING", font(9, "bold"), (255, 255, 255, 255))

    # action log (left side, 3 compact rows, translucent, expand chevron)
    ALX, ALW = 148, 470
    row_h, gap = 28, 3
    al_h = 3 * row_h + 2 * gap
    ALY = H - 12 - al_h
    log_strip = (ALX, ALY, ALX + ALW, ALY + al_h)
    shadow_box(log_strip, r=9)
    d.rounded_rectangle(log_strip, radius=9, fill=PAPER + (170,),
                        outline=(180, 188, 200, 150), width=1)
    # expand chevron (top right of the strip)
    cxx = ALX + ALW - 16
    cyy = ALY + row_h / 2
    d.polygon([(cxx - 5, cyy + 3), (cxx + 5, cyy + 3), (cxx, cyy - 4)],
              fill=INK2 + (220,))
    y1 = ALY + 8
    d.text((ALX + 12, y1), "Move:  Gerirr  2 squares", font=font(11),
           fill=INK + (235,))
    y1 += row_h + gap
    d.text((ALX + 12, y1 + 8), "Block:", font=font(11, "bold"), fill=INK + (235,))
    dx = ALX + 12 + text_w(d, "Block:", font(11, "bold")) + 8
    for v in (6, 2, 3):
        di = block_dice(v, 26)
        chrome.paste(di, (int(dx), int(y1 + 1)), di)
        dx += 31
    d.text((dx + 2, y1 + 8), "— success", font=font(11, "bold"),
           fill=GREEN + (255,))
    y1 += row_h + gap
    d.text((ALX + 12, y1 + 7), "Fumble:", font=font(12, "bold"), fill=RED + (255,))
    d.text((ALX + 12 + text_w(d, "Fumble:", font(12, "bold")) + 6, y1 + 8),
           "the ball is loose!", font=font(11), fill=INK + (235,))

    # chat button (bottom right) with unread badge
    cb = (W - 148, 702, W - 20, 746)
    shadow_box(cb, r=22)
    d.rounded_rectangle(cb, radius=22, fill=PAPER + (235,))
    d.ellipse((cb[0] + 12, cb[1] + 10, cb[0] + 30, cb[1] + 28), outline=INK + (255,), width=2)
    d.arc((cb[0] + 12, cb[1] + 10, cb[0] + 30, cb[1] + 28), 30, 150,
          fill=INK + (255,), width=2)
    d.text((cb[0] + 38, cb[1] + 10), "CHAT", font=font(13, "bold"), fill=INK + (255,))
    d.ellipse((cb[2] - 26, cb[1] - 8, cb[2] - 4, cb[1] + 14), fill=RED + (255,))
    center_text(d, cb[2] - 15, cb[1] - 6, "3", font(11, "bold"), (255, 255, 255, 255))

    # assemble
    out = canvas
    out.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(6)))
    out.alpha_composite(chrome)
    out_path = OUTDIR / cfg["out"]
    out.convert("RGB").save(out_path, "PNG")
    print("saved", out_path)

if __name__ == "__main__":
    sel = sys.argv[1] if len(sys.argv) > 1 else "all"
    keys = list(CONFIGS) if sel == "all" else [sel]
    for k in keys:
        try:
            compose(k)
        except FileNotFoundError as e:
            print(f"[{k}] not ready yet: {e}")
