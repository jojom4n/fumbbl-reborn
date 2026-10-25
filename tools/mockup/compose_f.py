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
  3. REAL FUMBBL data is composited (match 1956717 — Simply good (Orc) vs
     Plobando Latas (Skaven) — live-dumped via spectator_smoke; the WS
     serverGameState format, score from /api/match/current):
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
       * rain overlay only when the live weather is rain

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
HOME = (52, 108, 180)       # home (left) accent
AWAY = (193, 152, 55)       # away (right) accent
RED = (196, 60, 52)         # injury / failure ONLY
GREEN = (60, 142, 84)       # success
GOLD = (212, 175, 55)
AMBER = (186, 128, 24)      # caution (prone / stunned)
API_MATCH = Path("/tmp/mockF/api_cur.json")  # /api/match/current (same match)

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
    """Erase the AI's center line COMPLETELY (its whole bright extent, not a
    fixed ±9px) with local grass from both sides — otherwise the redrawn
    single line looks 'double'."""
    x0, y0, fw, fh = field
    a = np.asarray(im).astype(float)
    band = a[y0:y0+fh, x0:x0+fw, :].sum(axis=2)
    colmax = band.max(axis=0)
    cx_local = int(np.argmax(colmax[fw//2-halfwin:fw//2+halfwin])
                   + fw//2 - halfwin)
    cx = x0 + cx_local
    # grass reference (per-row mean, 10px strips 35-45px away from center)
    left_ref = a[y0:y0+fh, max(0, cx-45):max(1, cx-35), :].mean(axis=1)
    right_ref = a[y0:y0+fh, min(a.shape[1]-1, cx+35):min(a.shape[1], cx+45), :].mean(axis=1)
    grass = float(np.concatenate([left_ref, right_ref]).mean())
    thr = (float(colmax[cx_local - x0]) + grass) / 2.0
    # measure the contiguous bright extent of the AI line
    ext = 6
    for sgn in (-1, 1):
        x = cx
        while 0 <= x + sgn < a.shape[1] and ext < 50:
            x += sgn
            if colmax[x - x0] > thr:
                ext = max(ext, abs(x - cx))
            else:
                break
    ext += 3
    for dx in range(-ext, ext + 1):
        x = cx + dx
        if not (0 <= x < a.shape[1]):
            continue
        lo = max(0, cx - 40); hi = max(1, cx - 30)
        lo2 = min(a.shape[1]-1, cx + 30); hi2 = min(a.shape[1], cx + 40)
        L = a[y0:y0+fh, lo:hi, :].mean(axis=1)
        R = a[y0:y0+fh, lo2:hi2, :].mean(axis=1)
        t = 1.0 - min(1.0, abs(dx) / float(ext)) * 0.30  # >= 0.70 blend
        a[y0:y0+fh, x, :] = a[y0:y0+fh, x, :] * (1 - t) + (L + R) / 2 * t
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)), cx

def narrow_td_zone(im, field, banner_x, text="TOUCHDOWN"):
    """The official field TD zone is ONE single column (col 0 / col 25).
    The Qwen art painted a ~2-column-wide red banner, so:
      * rebuild the TD column (col 0/25): solid crimson + the banner's
        high-frequency texture + vertical TOUCHDOWN text (GW-mat style);
      * repaint everything between the TD column and the banner's inner
        edge (banner_x) back to grass, sampled from a clean neighbour
        grass column (the overhang can reach ~0.14cw into the 2nd/24th
        column, so the repaint must go all the way to banner_x)."""
    x0, y0, fw, fh = field
    a = np.asarray(im).astype(np.int16)
    cw = fw / 26
    for s in (0, 1):
        if s == 0:
            td0, td1 = int(x0), int(x0 + cw)
            g0, g1 = td1, int(banner_x[0])  # TD end .. banner inner edge
            src0 = int(x0 + 3 * cw)          # clean grass source column
        else:
            td0, td1 = int(x0 + 25 * cw), int(x0 + 26 * cw)
            g0, g1 = int(banner_x[1]), td0  # banner inner edge .. TD start
            src0 = int(x0 + 22 * cw)         # clean grass source column
        # 1) the banner overhang back to grass (sampled full-height strip,
        #    exactly the destination width)
        gw = g1 - g0
        grass = a[y0:y0+fh, src0:src0 + gw, :]
        rng = np.random.RandomState(5 + s)
        noise = rng.randint(-4, 5, (fh, gw, 3)).astype(np.int16)
        a[y0:y0+fh, g0:g1, :] = np.clip(grass + noise, 0, 255)
        # 2) TD column: solid crimson + fine matte grain (sampling the AI
        #    banner would ghost its big TOUCHDOWN letters onto the column)
        base = np.array([128, 30, 28], float)
        tdw = td1 - td0
        grain = rng.randint(-7, 8, (fh, tdw, 3))
        a[y0:y0+fh, td0:td1, :] = np.clip(base + grain, 0, 255)
    im = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    # 3) vertical TOUCHDOWN text, centered in each TD column
    t = Image.new("RGBA", (320, 60), (0, 0, 0, 0))
    dt = ImageDraw.Draw(t)
    f = font(34, "black")
    dt.text(((320 - text_w(dt, text, f)) / 2, 6), text, font=f,
            fill=(255, 255, 255, 235))
    t = t.rotate(90, expand=True)  # reads top-to-bottom
    im2 = im.convert("RGBA")
    for s in (0, 1):
        cx = x0 + (0.5 if s == 0 else 25.5) * cw  # middle of the TD column
        im2.alpha_composite(t, (int(cx - t.width / 2), int(y0 + (fh - t.height) / 2)))
    return im2.convert("RGB")

def grid_mow_tint(im, field, strength=0.85, light=1.20, dark=0.80, brighten=1.08):
    """Mowing stripes PERFECTLY aligned to the grid: every grass column is
    tinted light or dark (alternating), so each cell is clearly light-grass
    or dark-grass; plus a global slight brightening of the pitch (TD
    columns excluded — they keep their banner colour)."""
    x0, y0, fw, fh = field
    a = np.asarray(im).astype(float)
    cw = fw / 26.0
    for i in range(1, 25):  # grass columns only (0/25 = TD)
        c0, c1 = int(x0 + i * cw), int(x0 + (i + 1) * cw)
        m = (light if i % 2 == 0 else dark) * brighten
        seg = a[y0:y0+fh, c0:c1, :]
        a[y0:y0+fh, c0:c1, :] = np.clip(seg * (1 + (m - 1) * strength), 0, 255)
    return Image.fromarray(a.astype(np.uint8))

def draw_exact_grid(im, field, banner_x):
    """Crisp exact 26x15 grid across the WHOLE field: the two TD columns
    (col 0 / col 25, per the official field) are each a single column of
    15 visible cells, so horizontal lines run full width. Side-zone rows
    (row 0 / row 14) get slightly emphasized boundary lines. Single thick
    center line at the true midpoint (the i=13 grid line is skipped so it
    never renders double)."""
    x0, y0, fw, fh = field
    cw, ch = fw / 26, fh / 15
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    soft = (255, 255, 255, 110)
    side = (255, 255, 255, 150)
    # vertical lines i=1..25 (i=13 = center line, drawn separately)
    for i in range(1, 26):
        if i == 13:
            continue
        x = x0 + i * cw
        d.line((x, y0, x, y0 + fh), fill=soft, width=2)
    # horizontal lines j=0..15 across the FULL width (incl. TD columns)
    for j in range(0, 16):
        y = y0 + j * ch
        if j in (1, 14):  # side-zone boundary lines (rows 0 and 14)
            d.line((x0, y, x0 + fw, y), fill=side, width=3)
        else:
            d.line((x0, y, x0 + fw, y), fill=soft, width=2)
    # center line: SINGLE, emphasized
    cx = x0 + 13 * cw
    d.line((cx, y0, cx, y0 + fh), fill=(255, 255, 255, 240), width=5)
    im.paste(Image.alpha_composite(im.convert("RGBA"), layer).convert("RGB"))
    return cw, ch

# =================================================================== data
# PlayerState (ffb PlayerState.java): base = state & 0xFF: 1 STANDING,
# 2 MOVING, 3 PRONE, 4 STUNNED, 5 KNOCKED_OUT, 6 BADLY_HURT, 7 SERIOUS_INJURY,
# 8 RIP, 9 RESERVE, 10 MISSING  (bit 0x100 = active)
def _posmap(team):
    return {p["positionId"]: p["positionName"] for p in
            (team.get("roster") or {}).get("positionArray", [])}

def load_live():
    d = json.load(open(LIVE))
    g = d["game"]
    th, ta = g["teamHome"], g["teamAway"]
    hmap = {p["playerId"]: p for p in th["playerArray"]}
    amap = {p["playerId"]: p for p in ta["playerArray"]}
    hp, ap = _posmap(th), _posmap(ta)
    players = []
    for p in g["fieldModel"]["playerDataArray"]:
        x, y = p["playerCoordinate"]
        if x < 0 or x > 25 or y < 0 or y > 14:  # in the box
            continue
        src = hmap.get(p["playerId"]) or amap.get(p["playerId"])
        if not src:
            continue
        side = "H" if p["playerId"] in hmap else "A"
        players.append({
            "side": side, "name": src["playerName"], "nr": src["playerNr"],
            "x": x, "y": y, "state": p["playerState"] & 0xFF,
            "posId": src["positionId"],
            "posName": (hp if side == "H" else ap).get(src["positionId"], "Player"),
            "race": (th if side == "H" else ta)["race"],
        })
    boxes = {"H": [], "A": []}
    for side, team, mmap, pmap in (("H", th, hmap, hp), ("A", ta, amap, ap)):
        for p in team["playerArray"]:
            q = next((q for q in g["fieldModel"]["playerDataArray"]
                      if q["playerId"] == p["playerId"]), None)
            if not q:
                continue
            st = q["playerState"] & 0xFF
            if st in (5, 6, 7, 8, 9, 10):  # KO / hurt / SI / RIP / reserve / missing
                boxes[side].append((p["playerName"], st,
                                    pmap.get(p["positionId"], "Player")))
    # score from the match API (looked up by match id), fallback 0:0
    score = "0 : 0"
    try:
        for m in json.load(open(API_MATCH)):
            if m["id"] == g.get("gameId"):
                tm = {t["side"]: t for t in m["teams"]}
                score = f"{tm['home']['score']} : {tm['away']['score']}"
                break
    except Exception:
        pass
    # acting-player spotlight: server actingPlayer if set, else the closest
    # standing player of the team on turn to the ball (ties: more skills —
    # makes a nicer card for the mockup)
    ball = g["fieldModel"]["ballCoordinate"]
    ap_id = (g.get("actingPlayer") or {}).get("playerId")
    if ap_id:
        pd = next(q for q in g["fieldModel"]["playerDataArray"]
                  if q["playerId"] == ap_id)
    else:
        pm = hmap if g["homePlaying"] else amap
        cands = [q for q in g["fieldModel"]["playerDataArray"]
                 if q["playerCoordinate"][0] >= 0 and q["playerId"] in pm
                 and (q["playerState"] & 0xFF) in (1, 2)]
        cands.sort(key=lambda q: (abs(q["playerCoordinate"][0] - ball[0])
                                  + abs(q["playerCoordinate"][1] - ball[1]),
                                 -len(pm[q["playerId"]].get("skillArray") or [])))
        pd = cands[0]
    side = "H" if pd["playerId"] in hmap else "A"
    src = hmap[pd["playerId"]] if side == "H" else amap[pd["playerId"]]
    pmap = hp if side == "H" else ap
    acting = {
        "side": side, "name": src["playerName"], "nr": src["playerNr"],
        "x": pd["playerCoordinate"][0], "y": pd["playerCoordinate"][1],
        "state": pd["playerState"] & 0xFF, "posId": src["positionId"],
        "posName": pmap.get(src["positionId"], "Player"),
        "race": (th if side == "H" else ta)["race"],
        "skills": src.get("skillArray") or [],
        "mv": src.get("movement"), "st": src.get("strength"),
        "ag": src.get("agility"), "pa": src.get("armour"),
    }
    return {
        "home": th, "away": ta, "players": players, "boxes": boxes,
        "ball": ball, "ball_in_play": g["fieldModel"]["ballInPlay"],
        "weather": g["fieldModel"].get("weather"),
        "half": g["half"], "turn": g["turnDataHome"]["turnNr"],
        "home_playing": g["homePlaying"], "turn_time": g["turnTime"],
        "turn_limit": 240, "score": score, "acting": acting,
    }

# face slug per player (race + position; slugs from assets/faces/)
def face_for(race, pos_name):
    r, pl = race.lower(), pos_name.lower()
    if "wood elf" in r:
        if "thrower" in pl: return "wood_elf_thrower"
        if "catcher" in pl: return "wood_elf_catcher"
        if "blitzer" in pl: return "elf_blitzer"
        return "wood_elf_lineman"
    if "skaven" in r:
        if "thrower" in pl: return "skaven_thrower"
        if "blitzer" in pl: return "skaven_blitzer"
        return "skaven_clanrat"
    if "orc" in r:
        if "thrower" in pl: return "orc_thrower"
        if "blitzer" in pl: return "orc_blitzer"
        if "lineman" in pl: return "orc_lineman"
        return "black_orc"
    if "thrower" in pl: return "human_thrower"
    if "blitzer" in pl: return "human_blitzer"
    if "catcher" in pl: return "human_catcher"
    return "human_lineman"

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
    """Official GW block die: RED die, WHITE symbols (BB2025 set). Faces:
      1 skull (player down)      2 skull in burst (both down)
      3 chubby arrow (push)      4 chubby arrow (push, SAME symbol)
      5 '!' in burst (stumble)   6 burst (POW)
    Symbol shapes follow the official GW dice (verified on official 2025
    edition photos + the modern physical dice)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    RED = (176, 32, 28, 255)
    RED_D = (118, 16, 14, 255)
    WT = (255, 255, 255, 255)
    d.rounded_rectangle((1, 1, size - 2, size - 2), radius=size // 5,
                        fill=RED, outline=RED_D, width=1)
    c = size / 2

    def burst(cx, cy, r_out, r_in, n=11, rot=-math.pi / 2):
        pts = []
        for i in range(n * 2):
            rr = r_out if i % 2 == 0 else r_in
            ang = rot + i * math.pi / n
            pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
        return pts

    def skull(cx, cy, r, col, eye):
        # cranium + jaw
        d.ellipse((cx - r * 0.95, cy - r * 1.0, cx + r * 0.95, cy + r * 0.45), fill=col)
        d.rectangle((cx - r * 0.6, cy + r * 0.2, cx + r * 0.6, cy + r * 0.95), fill=col)
        # angular slanted eyes
        d.polygon([(cx - r * 0.68, cy - r * 0.35), (cx - r * 0.15, cy - r * 0.2),
                   (cx - r * 0.22, cy + r * 0.08), (cx - r * 0.68, cy + r * 0.0)], fill=eye)
        d.polygon([(cx + r * 0.68, cy - r * 0.35), (cx + r * 0.15, cy - r * 0.2),
                   (cx + r * 0.22, cy + r * 0.08), (cx + r * 0.68, cy + r * 0.0)], fill=eye)
        # nose + teeth gaps + crack
        d.polygon([(cx, cy + r * 0.12), (cx - r * 0.16, cy + r * 0.42),
                   (cx + r * 0.16, cy + r * 0.42)], fill=eye)
        tw = max(1, int(size * 0.05))
        for tx in (-0.38, -0.13, 0.13, 0.38):
            d.line((cx + tx * r, cy + r * 0.32, cx + tx * r, cy + r * 0.85), fill=eye, width=tw)
        d.line((cx + r * 0.3, cy - r * 0.95, cx + r * 0.12, cy - r * 0.6), fill=eye,
               width=max(1, int(size * 0.04)))

    def arrow_push(cx, cy, s):
        # chubby diagonal arrow with lightning tail (official push symbol)
        v = (math.cos(math.radians(45)), math.sin(math.radians(45)))
        p = (-v[1], v[0])
        tip = (cx + v[0] * s * 0.95, cy + v[1] * s * 0.95)
        bx = cx + v[0] * s * 0.35; by = cy + v[1] * s * 0.35
        d.polygon([tip, (bx + p[0] * s * 0.62, by + p[1] * s * 0.62),
                   (bx - p[0] * s * 0.62, by - p[1] * s * 0.62)], fill=WT)
        t0 = (bx + p[0] * s * 0.18, by + p[1] * s * 0.18)
        t1 = (cx - v[0] * s * 0.25 + p[0] * s * 0.32, cy - v[1] * s * 0.25 + p[1] * s * 0.32)
        t2 = (cx - v[0] * s * 0.55 - p[0] * s * 0.12, cy - v[1] * s * 0.55 - p[1] * s * 0.12)
        t3 = (cx - v[0] * s * 0.95 + p[0] * s * 0.10, cy - v[1] * s * 0.95 + p[1] * s * 0.10)
        d.line([t0, t1, t2, t3], fill=WT, width=max(2, int(s * 0.42)), joint="curve")

    if value == 1:
        skull(c, c + size * 0.02, size * 0.26, WT, RED)
    elif value == 2:
        d.polygon(burst(c, c, size * 0.46, size * 0.30), fill=WT)
        skull(c, c + size * 0.01, size * 0.20, RED, WT)
    elif value in (3, 4):  # SAME chubby arrow on both push faces
        arrow_push(c, c, size * 0.36)
    elif value == 5:
        d.polygon(burst(c, c, size * 0.46, size * 0.30), fill=WT)
        bw = max(2, int(size * 0.09))
        d.line((c, c - size * 0.14, c, c + size * 0.06), fill=RED, width=bw)
        d.ellipse((c - bw / 2, c + size * 0.12, c + bw / 2, c + size * 0.12 + bw), fill=RED)
    elif value == 6:
        d.polygon(burst(c, c, size * 0.48, size * 0.32, n=12), fill=WT)
    return img

def coin(d, cx, cy, r, active=True):
    d.ellipse((cx - r, cy - r, cx + r, cy + r),
              fill=(212, 175, 55, 255) if active else (196, 198, 204, 255),
              outline=(150, 118, 30, 255) if active else (160, 162, 170, 255), width=1)
    if active:
        d.ellipse((cx - r * 0.55, cy - r * 0.6, cx + r * 0.3, cy + r * 0.05),
                  fill=(240, 216, 130, 200))

def gold_emboss(d, x, y, s, fnt):
    """Team name of the team on turn: gold, raised (embossed)."""
    d.text((x, y + 2), s, font=fnt, fill=(122, 88, 22, 255))
    d.text((x, y), s, font=fnt, fill=GOLD + (255,))

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

def sun_icon(d, x, y, s=18):
    c = (x + s * 0.5, y + s * 0.5)
    d.ellipse((x + s * 0.28, y + s * 0.28, x + s * 0.72, y + s * 0.72),
              fill=(240, 196, 80, 255), outline=(190, 140, 40, 255), width=1)
    for k in range(8):
        ang = k * math.pi / 4
        d.line((x + s * (0.5 + 0.36 * math.cos(ang)), y + s * (0.5 + 0.36 * math.sin(ang)),
                x + s * (0.5 + 0.48 * math.cos(ang)), y + s * (0.5 + 0.48 * math.sin(ang))),
               fill=(240, 196, 80, 255), width=2)

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

def team_logo(path, size, alpha=0.19):
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
    field = narrow_td_zone(field, cfg["field"], cfg["banner_x"])  # TD = 1 column
    field = grid_mow_tint(field, cfg["field"])           # grid-aligned mowing bands
    cw, ch = draw_exact_grid(field, cfg["field"], cfg["banner_x"])
    canvas = field.convert("RGBA")

    def cell_rect(x, y):
        return (fx + x * cw, fy + y * ch, fx + (x + 1) * cw, fy + (y + 1) * ch)
    def cell_center(x, y):
        return (fx + (x + 0.5) * cw, fy + (y + 0.5) * ch)

    # --- 2. TEAM LOGOS: per-match real logos, ~19% alpha (round 7: less
    # transparent than round 6's 13%), center of each half
    logo_sz = int(fh * 0.52)
    for path, col in ((LOGOS / "h_big.png", 6.5), (LOGOS / "a_big.png", 19.5)):
        cx = fx + (col) * cw
        cy = fy + fh / 2
        lg = team_logo(path, logo_sz, alpha=0.19)
        canvas.paste(lg, (int(cx - logo_sz / 2), int(cy - logo_sz / 2)), lg)

    d = ImageDraw.Draw(canvas)

    # --- 3. target brackets (BB2 style) on two empty cells adjacent to the
    # acting player (prefer the cell towards the ball, then the ones ahead)
    act = live["acting"]
    occupied = {(p["x"], p["y"]) for p in live["players"]}
    occupied.add(tuple(live["ball"]))
    bx_, by_ = act["x"], act["y"]
    bdir = -1 if live["ball"][0] < bx_ else (1 if live["ball"][0] > bx_ else 0)
    neigh = [(bx_ + s, by_) for s in (bdir, -bdir if bdir else 1)] \
            + [(bx_, by_ - 1), (bx_, by_ + 1)]
    picks = []
    for (nx, ny) in neigh:
        if 0 <= nx <= 25 and 0 <= ny <= 14 and (nx, ny) not in occupied \
                and (nx, ny) not in picks:
            picks.append((nx, ny))
        if len(picks) == 2:
            break
    for (tx, ty) in picks:
        target_brackets(d, cell_rect(tx, ty))

    # --- 4. white action wheel on the acting player (under the token)
    ax, ay = cell_center(act["x"], act["y"])
    draw_wheel(canvas, ax, ay, min(cw, ch))
    d = ImageDraw.Draw(canvas)

    # --- 5. tokens (real coordinates, real faces, real states)
    ball = live["ball"]
    for p in live["players"]:
        ring = HOME if p["side"] == "H" else AWAY
        slug = face_for(p["race"], p["posName"])
        st = p["state"]
        prone = (st == 3)  # PRONE (FFB PlayerState base)
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

    # --- 6. rain (MicroProse-Soccer style: fine top-down droplet specks) —
    # only when the live weather is rain
    if "rain" in (live.get("weather") or "").lower():
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

    crest_h = Image.open(LOGOS / "h_small.png").convert("RGBA").resize((40, 40), Image.LANCZOS)
    crest_a = Image.open(LOGOS / "a_small.png").convert("RGBA").resize((40, 40), Image.LANCZOS)
    chrome.paste(crest_h, (14, 11), crest_h)
    chrome.paste(crest_a, (W - 54, 11), crest_a)
    d = ImageDraw.Draw(chrome)

    nm_h = th["teamName"].upper()
    nm_a = ta["teamName"].upper()
    # team on turn: its name in GOLD, raised (embossed) — the other stays plain
    if live["home_playing"]:
        gold_emboss(d, 64, 8, nm_h, font(14, "bold"))
        d.text((W - 64 - text_w(d, nm_a, font(14, "bold")), 8), nm_a,
               font=font(14, "bold"), fill=INK + (255,))
    else:
        d.text((64, 8), nm_h, font=font(14, "bold"), fill=INK + (255,))
        gold_emboss(d, W - 64 - text_w(d, nm_a, font(14, "bold")), 8, nm_a,
                    font(14, "bold"))
    d.text((64, 28), f"{th['coach']}  ·  {th['race']}", font=font(10), fill=INK2 + (255,))
    sub_a = f"{ta['coach']}  ·  {ta['race']}"
    d.text((W - 64 - text_w(d, sub_a, font(10)), 28), sub_a, font=font(10),
           fill=INK2 + (255,))

    # rerolls: dots (3 slots, filled = remaining)
    n_rh, n_ra = th["reRolls"], ta["reRolls"]
    x0 = 64 + text_w(d, nm_h, font(14, "bold")) + 16
    for i in range(3):
        coin(d, x0 + i * 19, 17, 7, active=i < n_rh)
    x0 = W - 64 - 3 * 19 - 16 - text_w(d, nm_a, font(14, "bold"))
    for i in range(3):
        coin(d, x0 + i * 19, 17, 7, active=i < n_ra)

    # END TURN button next to the active human coach (the team on turn) —
    # round 7: a little lower (pill y 32..54, was 28..50)
    sub_h = f"{th['coach']}  ·  {th['race']}"
    if live["home_playing"]:
        ex = 64 + text_w(d, sub_h, font(10)) + 14
        d.rounded_rectangle((ex, 32, ex + 82, 54), radius=11, fill=HOME + (255,),
                            outline=tuple(max(0, c - 60) for c in HOME) + (255,),
                            width=1)
        d.text((ex + 8, 37), "END TURN", font=font(10, "bold"),
               fill=(255, 255, 255, 255))
    else:
        ex = W - 64 - 82 - text_w(d, sub_a, font(10)) - 14
        d.rounded_rectangle((ex, 32, ex + 82, 54), radius=11, fill=AWAY + (255,),
                            outline=tuple(max(0, c - 60) for c in AWAY) + (255,),
                            width=1)
        d.text((ex + 8, 37), "END TURN", font=font(10, "bold"),
               fill=(255, 255, 255, 255))

    # center: score + one clean row: [TURN n][HALF]  time  weather
    # (the FUMBBL turn timer COUNTS UP — no decreasing progress bar)
    score = live["score"]
    fs = font(22, "black")
    d.text((W / 2 - text_w(d, score, fs) / 2, 5), score, font=fs, fill=INK + (255,))
    ty = 34
    el = int(live["turn_time"] / 1000)
    t = f"{el // 60}:{el % 60:02d}"
    wx_txt = live.get("weather") or "Nice Weather"
    w1 = text_w(d, f"TURN {live['turn']}", font(10, "bold")) + 14
    w2 = text_w(d, f"{live['half']}ST HALF", font(10, "bold")) + 14
    w3 = text_w(d, t, font(12, "black"))
    w4 = 26 + text_w(d, wx_txt, font(10))
    total = w1 + 8 + w2 + 16 + w3 + 16 + w4
    x = W / 2 - total / 2
    d.rounded_rectangle((x, ty, x + w1, ty + 16), radius=8, outline=HOME + (200,), width=1)
    d.text((x + 7, ty + 2), f"TURN {live['turn']}", font=font(10, "bold"), fill=HOME + (255,))
    x += w1 + 8
    d.rounded_rectangle((x, ty, x + w2, ty + 16), radius=8, outline=INK2 + (200,), width=1)
    d.text((x + 7, ty + 2), f"{live['half']}ST HALF", font=font(10, "bold"), fill=INK2 + (255,))
    x += w2 + 16
    d.text((x, ty - 2), t, font=font(12, "black"), fill=INK + (255,))
    x += w3 + 16
    if "rain" in wx_txt.lower():
        rain_icon(d, x, ty - 3, s=18)
    else:
        sun_icon(d, x, ty - 3, s=18)
    d.text((x + 26, ty + 2), wx_txt, font=font(10), fill=INK2 + (255,))

    # box strips (ultra-thin vertical, left = home, right = away)
    def box_strip(x, team, color, race, box_players):
        # FFB PlayerState bases: 9 RESERVE, 5 KNOCKED_OUT,
        # 6 BADLY_HURT / 7 SERIOUS_INJURY / 8 RIP / 10 MISSING
        bx = (x, 120, x + 46, 470)
        shadow_box(bx, r=12)
        d.rounded_rectangle(bx, radius=12, fill=PAPER + (205,))
        d.rounded_rectangle((x + 4, 124, x + 42, 130), radius=3, fill=color + (220,))
        y = 140
        for label, codes in (("RESERVE", (9,)), ("KO", (5,)),
                             ("INJURED", (6, 7, 8, 10))):
            d.text((x + 8, y), label, font=font(7, "bold"), fill=INK2 + (220,))
            y += 13
            mine = [b for b in box_players if b[1] in codes]
            n = max(2, len(mine))
            for i in range(n):
                sy = y + i * 24 + 12
                if i < len(mine):
                    nm, _, pname = mine[i]
                    slug = face_for(race, pname)
                    face = Image.open(FACES / f"{slug}.png").convert("RGBA")
                    face = face.resize((20, 20), Image.LANCZOS)
                    mask = Image.new("L", face.size, 0)
                    ImageDraw.Draw(mask).ellipse((0, 0, 20, 20), fill=255)
                    chrome.paste(face, (x + 23 - 10, sy - 10), mask)
                    d.ellipse((x + 10, sy - 12, x + 36, sy + 12),
                              outline=color + (230,), width=2)
                else:
                    d.ellipse((x + 10, sy - 12, x + 36, sy + 12),
                              outline=(160, 166, 178, 160), width=2)
            y += n * 24 + 6

    box_strip(8, "H", HOME, th["race"], live["boxes"]["H"])
    box_strip(W - 54, "A", AWAY, ta["race"], live["boxes"]["A"])

    # player card (the acting player; placed on the side opposite his cell)
    ac = live["acting"]
    card_team = th if ac["side"] == "H" else ta
    ACC = HOME if ac["side"] == "H" else AWAY
    CW_, CH_ = 224, 316
    CX0 = 66 if ac["x"] >= 13 else W - 54 - CW_ - 12
    CY0 = 96
    card = (CX0, CY0, CX0 + CW_, CY0 + CH_)
    shadow_box(card, r=14)
    d.rounded_rectangle(card, radius=14, fill=PAPER + (232,))
    d.rounded_rectangle(card, radius=14, outline=ACC + (160,), width=1)
    d.rectangle((CX0 + 4, CY0 + 12, CX0 + 8, CY0 + CH_ - 12), fill=ACC + (230,))
    cxm = CX0 + CW_ // 2 + 2
    fsz = 88
    face = Image.open(FACES / f"{face_for(ac['race'], ac['posName'])}.png") \
        .convert("RGBA").resize((fsz, fsz), Image.LANCZOS)
    mask = Image.new("L", face.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, fsz, fsz), fill=255)
    chrome.paste(face, (cxm - fsz // 2, CY0 + 14), mask)
    d = ImageDraw.Draw(chrome)
    d.ellipse((cxm - fsz // 2, CY0 + 14, cxm + fsz // 2, CY0 + 14 + fsz),
              outline=ACC + (220,), width=2)
    center_text(d, cxm, CY0 + 108, ac["name"].upper(), font(15, "bold"), INK + (255,))
    center_text(d, cxm, CY0 + 128, f"#{ac['nr']}  ·  {ac['posName']}", font(10), INK2 + (255,))
    center_text(d, cxm, CY0 + 142, card_team["teamName"], font(9), INK2 + (235,))
    d.line((CX0 + 16, CY0 + 160, CX0 + CW_ - 16, CY0 + 160), fill=(0, 0, 0, 40), width=1)
    stats = [("MA", ac["mv"]), ("ST", ac["st"]), ("AG", ac["ag"]),
             ("PA", ac["pa"]), ("AV", ac["pa"] + ac["st"])]
    colw = (CW_ - 24) // 5
    for i, (lb, v) in enumerate(stats):
        px = CX0 + 12 + i * colw + colw // 2
        center_text(d, px, CY0 + 168, lb, font(9, "bold"), INK2 + (235,))
        center_text(d, px, CY0 + 182, str(v), font(16, "black"), INK + (255,))
    sy = CY0 + 212
    center_text(d, cxm, sy, "SKILLS", font(8, "bold"), INK2 + (235,))
    sx = CX0 + 14
    for sk in ac["skills"][:3]:
        lbl = sk.upper()
        tw_ = text_w(d, lbl, font(9, "bold"))
        d.rounded_rectangle((sx, sy + 14, sx + tw_ + 14, sy + 34), radius=10,
                            fill=(238, 240, 246, 255), outline=ACC + (150,), width=1)
        d.text((sx + 7, sy + 16), lbl, font=font(9, "bold"), fill=INK + (245,))
        sx += tw_ + 22
    # state only (Standing / Prone / Stunned) — no "ACTING" chip
    st_txt = {1: "STANDING", 2: "MOVING", 3: "PRONE", 4: "STUNNED"}.get(
        ac["state"], "STANDING")
    st_col = GREEN if ac["state"] in (1, 2) else AMBER
    center_text(d, cxm, CY0 + CH_ - 44, st_txt, font(12, "black"), st_col + (255,))

    # action log — flush in the bottom-left CORNER, very translucent,
    # truly bold (black weight), dark expand chevron
    ALX, ALW = 16, 470
    row_h, gap = 28, 3
    al_h = 3 * row_h + 2 * gap + 12
    ALY = H - 12 - al_h
    log_strip = (ALX, ALY, ALX + ALW, ALY + al_h)
    shadow_box(log_strip, r=9)
    d.rounded_rectangle(log_strip, radius=9, fill=PAPER + (95,),
                        outline=(180, 188, 200, 70), width=1)
    # expand chevron (top right of the strip): dark and clearly visible
    cxx = ALX + ALW - 16
    cyy = ALY + row_h / 2
    d.polygon([(cxx - 7, cyy + 5), (cxx + 7, cyy + 5), (cxx, cyy - 5)],
              fill=INK + (255,))
    y1 = ALY + 6
    d.text((ALX + 12, y1 + 1), f"Move:  {ac['name']}  2 squares", font=font(11, "black"),
           fill=INK + (255,))
    y1 += row_h + gap
    # block participants: two real players of the acting team near the ball
    bball = tuple(live["ball"])
    act_ps = sorted((p for p in live["players"] if p["side"] == act["side"]),
                    key=lambda p: abs(p["x"] - bball[0]) + abs(p["y"] - bball[1]))
    b1, b2 = act_ps[0]["name"], act_ps[1]["name"]
    blk = f"Block:  {b1} + {b2}"
    fb = font(10, "black")
    d.text((ALX + 12, y1 + 9), blk, font=fb, fill=INK + (235,))
    dx = ALX + 12 + text_w(d, blk, fb) + 10
    for v in (6, 2, 3):
        di = block_dice(v, 26)
        chrome.paste(di, (int(dx), int(y1 + 2)), di)
        dx += 31
    d.text((dx + 4, y1 + 9), "— success", font=fb, fill=GREEN + (255,))
    y1 += row_h + gap
    on_turn = th["teamName"] if live["home_playing"] else ta["teamName"]
    d.text((ALX + 12, y1 + 9), f"Turn {live['turn']}:  {on_turn} on the ball",
           font=font(11, "black"), fill=INK2 + (255,))

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
