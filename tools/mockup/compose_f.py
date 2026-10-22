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
    soft = (255, 255, 255, 46)
    # vertical lines (skip the ones under the banners)
    for i in range(1, 26):
        x = x0 + i * cw
        if banner_x[0] < x < banner_x[1]:
            d.line((x, y0, x, y0 + fh), fill=soft, width=1)
    # horizontal lines
    for j in range(0, 16):
        y = y0 + j * ch
        d.line((banner_x[0], y, banner_x[1], y), fill=soft, width=1)
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

_PIPS = {
    1: [(0.5, 0.5)],
    2: [(0.3, 0.3), (0.7, 0.7)],
    3: [(0.28, 0.28), (0.5, 0.5), (0.72, 0.72)],
    4: [(0.3, 0.3), (0.7, 0.3), (0.3, 0.7), (0.7, 0.7)],
    5: [(0.3, 0.3), (0.7, 0.3), (0.5, 0.5), (0.3, 0.7), (0.7, 0.7)],
    6: [(0.3, 0.26), (0.7, 0.26), (0.3, 0.5), (0.7, 0.5), (0.3, 0.74), (0.7, 0.74)],
}

def dice(value, size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((1, 1, size - 2, size - 2), radius=4,
                        fill=(255, 255, 255, 255), outline=(90, 96, 108, 255), width=1)
    pr = max(2, size // 9)
    for px, py in _PIPS[value]:
        cx, cy = int(2 + px * (size - 4)), int(2 + py * (size - 4))
        d.ellipse((cx - pr, cy - pr, cx + pr, cy + pr), fill=(30, 34, 42, 255))
    return img

def coin(d, cx, cy, r, active=True):
    d.ellipse((cx - r, cy - r, cx + r, cy + r),
              fill=(212, 175, 55, 255) if active else (196, 198, 204, 255),
              outline=(150, 118, 30, 255) if active else (160, 162, 170, 255), width=1)
    if active:
        d.ellipse((cx - r * 0.55, cy - r * 0.6, cx + r * 0.3, cy + r * 0.05),
                  fill=(240, 216, 130, 200))

def rain_icon(d, x, y, s=20):
    d.ellipse((x, y + s * 0.25, x + s * 0.75, y + s * 0.85), fill=(168, 180, 196, 255))
    d.ellipse((x + s * 0.25, y, x + s * 0.95, y + s * 0.6), fill=(168, 180, 196, 255))
    d.ellipse((x + s * 0.55, y + s * 0.25, x + s, y + s * 0.8), fill=(168, 180, 196, 255))
    for dx in (0.2, 0.5, 0.8):
        x0 = x + s * dx
        d.line((x0, y + s * 0.9, x0 - s * 0.08, y + s * 1.3),
               fill=(90, 140, 200, 255), width=2)

def draw_wheel(d, cx, cy, cell):
    R = cell * 1.75
    r = max(11, cell * 0.30)
    icons = ["move", "block", "blitz", "pass", "foul", "attack"]
    for i, name in enumerate(icons):
        ang = math.radians(-90 + i * 60)
        bx = cx + R * math.cos(ang)
        by = cy + R * math.sin(ang)
        d.ellipse((bx - r, by - r, bx + r, by + r),
                  fill=(26, 28, 34, 235), outline=(255, 255, 255, 130), width=1)
        s = r * 0.55
        if name == "move":
            d.line((bx, by + s, bx, by - s), fill=(255, 255, 255, 255), width=3)
            d.polygon([(bx - s * 0.6, by - s * 0.4), (bx + s * 0.6, by - s * 0.4),
                       (bx, by - s * 1.1)], fill=(255, 255, 255, 255))
        elif name == "block":
            d.polygon([(bx - s * 0.7, by - s * 0.7), (bx + s * 0.7, by - s * 0.7),
                       (bx + s * 0.7, by + s * 0.1), (bx, by + s * 0.9),
                       (bx - s * 0.7, by + s * 0.1)],
                      outline=(255, 255, 255, 255), width=2)
        elif name == "blitz":
            d.polygon([(bx + s * 0.2, by - s), (bx - s * 0.5, by + s * 0.15),
                       (bx - 0.5, by + s * 0.15), (bx - s * 0.2, by + s),
                       (bx + s * 0.5, by - s * 0.1), (bx + 0.5, by - s * 0.1)],
                      fill=(255, 255, 255, 255))
        elif name == "pass":
            d.arc((bx - s, by - s, bx + s, by + s), 90, 360,
                  fill=(255, 255, 255, 255), width=3)
            d.polygon([(bx + s, by - s * 0.2), (bx + s, by + s * 0.5),
                       (bx + s * 0.3, by - s * 0.1)], fill=(255, 255, 255, 255))
        elif name == "foul":
            for dx in (-s * 0.5, 0, s * 0.5):
                d.line((bx + dx, by - s * 0.8, bx + dx, by + s * 0.3),
                       fill=(255, 255, 255, 255), width=3)
            d.line((bx - s * 0.5, by + s * 0.5, bx + s * 0.5, by + s * 0.5),
                   fill=(255, 255, 255, 255), width=3)
        elif name == "attack":
            d.line((bx - s * 0.7, by - s * 0.7, bx + s * 0.7, by + s * 0.7),
                   fill=(255, 255, 255, 255), width=3)
            d.line((bx + s * 0.7, by - s * 0.7, bx - s * 0.7, by + s * 0.7),
                   fill=(255, 255, 255, 255), width=3)

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
           "out": "FINAL_F_71.png"},
    "72": {"in": "/home/saruman/ComfyUI-Shared/output/mockups/F_field_s72_00001_.png",
           "field": (51, 23, 1244, 705), "banner_x": (131, 1212),
           "out": "FINAL_F_72.png"},
}

def compose(key):
    cfg = CONFIGS[key]
    live = load_live()
    th, ta = live["home"], live["away"]

    # --- 1. field: clean the AI grid, draw the exact 26x15
    field = Image.open(cfg["in"]).convert("RGB")
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
        lg = team_logo(path, logo_sz, alpha=0.16)
        canvas.paste(lg, (int(cx - logo_sz / 2), int(cy - logo_sz / 2)), lg)

    d = ImageDraw.Draw(canvas)

    # --- 3. target brackets (BB2 style) on empty cells ahead of the thrower
    for (tx, ty) in ((12, 8), (13, 8)):
        target_brackets(d, cell_rect(tx, ty))

    # --- 4. tokens (real coordinates, real faces, real states)
    ball = live["ball"]
    ball_xy = None
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
        if acting:
            rr = size * 0.56
            d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr),
                      outline=(255, 255, 255, 235), width=2)
            ball_xy = (cx, cy)
    # loose ball (drawn peeking from under the token standing on the square)
    if live["ball_in_play"] and ball:
        ox, oy = cell_center(ball[0], ball[1])
        d.ellipse((ox + cw * 0.10, oy + ch * 0.22, ox + cw * 0.10 + cw * 0.44,
                   oy + ch * 0.22 + ch * 0.32),
                  fill=(139, 90, 43, 255), outline=(64, 38, 18, 255), width=1)

    # --- 5. dark radial wheel on the acting thrower
    ax, ay = cell_center(11, 8)
    draw_wheel(d, ax, ay, min(cw, ch))

    # --- 6. rain streaks (weather: Pouring Rain)
    rng = random.Random(7)
    for _ in range(150):
        x0 = rng.uniform(0, W)
        y0 = rng.uniform(0, H)
        L = rng.uniform(14, 30)
        a = rng.randint(8, 16)
        d.line((x0, y0, x0 - L * 0.22, y0 + L), fill=(235, 240, 250, a), width=1)

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
    x0 = 64 + text_w(d, nm_h, font(14, "bold")) + 16
    for i in range(3):
        coin(d, x0 + i * 19, 17, 7, active=i < n_rh)
    x0 = W - 64 - 3 * 19 - 16 - text_w(d, nm_a, font(14, "bold"))
    for i in range(3):
        coin(d, x0 + i * 19, 17, 7, active=i < n_ra)

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
    d.text((bx0 - 2, ty - 12), "END OF TURN", font=font(8, "bold"), fill=INK2 + (255,))
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

    # ticker + expanded history (bottom center, BB2 style, real dice)
    TW_ = 560
    TX = (W - TW_) // 2
    hist = [(TX, 610, TX + TW_, 650), (TX, 656, TX + TW_, 696)]
    for hb in hist:
        shadow_box(hb, r=9)
    d.rounded_rectangle(hist[0], radius=9, fill=PAPER + (210,))
    d.rounded_rectangle(hist[1], radius=9, fill=PAPER + (210,))
    d = ImageDraw.Draw(chrome)
    y1 = hist[0][1] + 8
    d.text((TX + 12, y1), "Block:", font=font(12, "bold"), fill=INK + (255,))
    dx = TX + 12 + text_w(d, "Block:", font(12, "bold")) + 8
    for v in (5, 6, 3):
        chrome.paste(dice(v, 24), (dx, y1 - 2), dice(v, 24))
        dx += 30
    d = ImageDraw.Draw(chrome)
    d.text((dx + 4, y1), "— success", font=font(12, "bold"), fill=GREEN + (255,))
    d.text((TX + 12, hist[1][1] + 8), "Move: Gerirr 2 squares", font=font(12),
           fill=INK + (255,))
    tick = (TX, 702, TX + TW_, 746)
    shadow_box(tick, r=10)
    d.rounded_rectangle(tick, radius=10, fill=PAPER + (232,))
    d.rounded_rectangle(tick, radius=10, outline=RED + (90,), width=1)
    d.ellipse((TX + 12, 718, TX + 20, 726), fill=RED + (255,))
    d.text((TX + 28, 715), "Fumble:", font=font(13, "bold"), fill=RED + (255,))
    d.text((TX + 28 + text_w(d, "Fumble:", font(13, "bold")) + 8, 715),
           "the ball is loose!", font=font(13), fill=INK + (255,))
    d.polygon([(TX + TW_ - 24, 726), (TX + TW_ - 14, 726), (TX + TW_ - 19, 718)],
              fill=INK2 + (255,))

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
