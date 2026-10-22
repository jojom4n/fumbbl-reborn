#!/usr/bin/env python3
"""Hybrid mockup compositor: AI chrome image + REAL FieldView + PIL-drawn UI.

Composes the AI-generated panel/layout image with:
  * the real FieldView rendering (exact 26x15 grid, real faces/tokens)
  * team boxes (reserve / KO / injured) along the top and bottom of the field
  * a strongly translucent player card on a screen edge (hover/click style, BB3)
  * crisp, real-font panel titles (Z-Image text is garbled; titles are redrawn)

Usage:
  python3 composite.py --config <name>            # uses per-image configs below
  (configs live in the CONFIGS dict at the bottom)
"""
import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = Path(__file__).parent
REPO = HERE.parent.parent
FIELD_PNG = "/tmp/field_real.png"
FACES = REPO / "assets" / "faces"
FIXTURE = REPO / "tests" / "serverGameState_fixture.json"
OUTDIR = REPO / "faces-work" / "mockups"

# ---------------------------------------------------------------- fonts

def find_font(names):
    import glob
    for n in names:
        hits = glob.glob(f"/usr/share/fonts/**/{n}", recursive=True)
        if hits:
            return hits[0]
    raise RuntimeError(f"no font found for {names}")

F_REG = find_font(["NotoSans-Regular.ttf", "DejaVuSans.ttf", "LiberationSans-Regular.ttf"])
F_BOLD = find_font(["NotoSans-Bold.ttf", "DejaVuSans-Bold.ttf", "LiberationSans-Bold.ttf"])


def font(size, bold=False):
    return ImageFont.truetype(F_BOLD if bold else F_REG, size)


# ---------------------------------------------------------------- helpers

def rounded_rect(draw, box, radius, fill=None, outline=None, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)


def face_image(slug, size):
    """Circular face with a dark ring, or an empty dashed slot."""
    p = FACES / f"{slug}.png"
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if p.exists():
        f = Image.open(p).convert("RGBA")
        f = f.resize((size - 6, size - 6), Image.LANCZOS)
        mask = Image.new("L", f.size, 0)
        ImageDraw.Draw(mask).ellipse((0, 0, f.size[0], f.size[1]), fill=255)
        img.paste(f, (3, 3), mask)
        d.ellipse((1, 1, size - 2, size - 2), outline=(20, 24, 32, 255), width=3)
    else:
        d.ellipse((2, 2, size - 3, size - 3), outline=(90, 100, 120, 160), width=2)
    return img


def text_w(draw, s, fnt):
    b = draw.textbbox((0, 0), s, font=fnt)
    return b[2] - b[0]


def center_text(draw, cx, y, s, fnt, fill):
    draw.text((cx - text_w(draw, s, fnt) / 2, y), s, font=fnt, fill=fill)


# ---------------------------------------------------------------- elements

def paste_field(canvas, field_rect):
    """Paste the real field into (x, y, w, h), keeping 26:15, feathered edges."""
    x, y, w, h = [int(v) for v in field_rect]
    fr = 26 / 15
    fw, fh = w, int(w / fr)
    if fh > h:
        fh, fw = h, int(h * fr)
    field = Image.open(FIELD_PNG).convert("RGBA")
    field = field.resize((fw, fh), Image.LANCZOS)
    # feathered mask so the AI edges (parchment/glass) blend in:
    # opaque inside, feathered rounded corners on the outside
    mask = Image.new("L", (fw, fh), 0)
    dm = ImageDraw.Draw(mask)
    feather = max(4, min(fw, fh) // 45)
    dm.rounded_rectangle((0, 0, fw - 1, fh - 1), radius=feather, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(feather / 2))
    cx = x + (w - fw) // 2
    cy = y + (h - fh) // 2
    canvas.paste(field, (cx, cy), mask)
    return (cx, cy, fw, fh)


def draw_boxes(canvas, field_rect, side, team_color, label_color, slugs_by_zone,
               tag=""):
    """One row of box slots (reserve/ko/injured) drawn ON the field's end row
    (row 0 or row 14 — always empty), so the boxes read as attached to the
    field, like the tabletop box. Slots: team-colored rings; groups separated
    by a wider gap; a small tag (HOME/AWAY) marks the row."""
    x, y, w, h = [int(v) for v in field_rect]
    cw = canvas.convert("RGBA")
    draw = ImageDraw.Draw(cw)
    cell_h = h / 15
    slot = min(34, max(20, int(cell_h) - 2))
    gap = int(slot * 1.45)
    groups = []  # (label, [slug or None])
    for label in ("RESERVE", "KO", "INJURED"):
        groups.append((label, slugs_by_zone.get(label, [])))
    total = sum(len(s) for _, s in groups) * gap + (len(groups) - 1) * 16
    tag_w = 74 if tag else 0
    gx = x + tag_w + (w - tag_w) // 2 - total // 2
    if side == "top":
        gy = y + int(cell_h / 2) - slot // 2
    else:
        gy = y + h - int(cell_h) + int(cell_h / 2) - slot // 2
    for gi, (label, slugs) in enumerate(groups):
        for i, slug in enumerate(slugs):
            sx = gx + (sum(len(s) for _, s in groups[:gi]) + i) * gap + gi * 16
            img = face_image(slug, slot)
            cw.paste(img, (sx - slot // 2, gy), img)
        gx += len(slugs) * gap + 16
    if tag:
        d2 = font(11, True)
        draw.text((x + 10, gy + slot // 2 - 7), tag, font=d2, fill=label_color)
    canvas.paste(cw)


def cover_rects(canvas, rects, fill=(10, 14, 22, 235)):
    """Paint over panels that the new design removes (roster lists, old-style
    corner player card) — reads as 'empty dock area'."""
    cw = canvas.convert("RGBA")
    d = ImageDraw.Draw(cw)
    for r in rects:
        rounded_rect(d, r, 10, fill=fill)
    canvas.paste(cw)


def draw_player_card(canvas, edge, fixture, pos=None):
    """Strongly translucent player card on the left or right screen edge."""
    team = fixture["game"]["teamHome"]
    player = team["playerArray"][0]
    stats = {"MA": player["movement"], "ST": player["strength"],
             "AG": player["agility"], "PA": player.get("armour", 0),
             "AV": player.get("armour", 0)}
    skills = player.get("skillArray", [])[:3]

    W, H = 216, 372
    if pos:
        x, y = pos
    else:
        x = 12 if edge == "left" else canvas.width - W - 12
        y = (canvas.height - H) // 2
    cw = canvas.convert("RGBA")
    card = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(card)

    # panel body — STRONGLY translucent (alpha ~45%)
    rounded_rect(d, (0, 0, W - 1, H - 1), 14, fill=(8, 12, 22, 115),
                 outline=(130, 165, 225, 110), width=2)
    # team accent
    accent = (200, 164, 92)  # gold (home)
    d.rectangle((0, 14, 5, H - 14), fill=accent + (170,))

    cx = W // 2
    # portrait
    slug = "orc_lineman"
    f = Image.open(FACES / f"{slug}.png").convert("RGBA").resize((92, 92), Image.LANCZOS)
    mask = Image.new("L", (92, 92), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, 92, 92), fill=255)
    card.paste(f, (cx - 46, 14), mask)
    d.ellipse((cx - 46, 14, cx + 46, 106), outline=accent + (200,), width=2)

    # name / number / team
    num = player.get("playerNr", "?")
    name = player.get("playerName", "Player")
    team = team.get("teamName", "")
    center_text(d, cx, 116, name, font(19, True), (240, 244, 252, 235))
    center_text(d, cx, 142, f"#{num}  {team}", font(12), (168, 184, 208, 220))
    d.line((16, 166, W - 16, 166), fill=(130, 165, 225, 90), width=1)

    # stats grid
    sy = 182
    labels = ["MA", "ST", "AG", "PA", "AV"]
    vals = [stats.get(k, 0) for k in labels]
    colw = (W - 32) // 5
    for i, (lb, v) in enumerate(zip(labels, vals)):
        px = 16 + i * colw
        center_text(d, px + colw // 2, sy, lb, font(10, True), (150, 168, 196, 210))
        center_text(d, px + colw // 2, sy + 15, str(v), font(19, True), (245, 248, 255, 240))

    # skill chips
    sy2 = sy + 52
    if skills:
        center_text(d, cx, sy2, "SKILLS", font(10, True), (150, 168, 196, 210))
        sx = 14
        for sk in skills[:4]:
            lbl = sk.upper()
            tw = text_w(d, lbl, font(11, True))
            rounded_rect(d, (sx, sy2 + 18, sx + tw + 16, sy2 + 40), 10,
                         fill=(30, 40, 60, 160), outline=accent + (180,), width=1)
            center_text(d, sx + (tw + 16) // 2, sy2 + 22, lbl, font(11, True),
                        (235, 230, 210, 230))
            sx += tw + 24
    # status line
    center_text(d, cx, H - 44, "STANDING", font(11, True),
                (140, 200, 150, 210))
    center_text(d, cx, H - 24, "Ready to act", font(12), (190, 210, 230, 210))

    cw.paste(card, (x, y), card)
    canvas.paste(cw)


def fix_title(canvas, xy, text, size=15, bg=(10, 14, 24, 210), fg=(235, 240, 250, 255),
              pad=(10, 6), bold=True, align="left", width=None):
    """Overpaint a garbled AI title with crisp real-font text."""
    x, y = xy
    cw = canvas.convert("RGBA")
    d = ImageDraw.Draw(cw)
    fnt = font(size, bold)
    tw = text_w(d, text, fnt)
    bh = size + pad[1] * 2
    bx0 = x - pad[0]
    if align == "center":
        bx0 = x - tw // 2 - pad[0]
    bx1 = bx0 + tw + pad[0] * 2
    if width:
        bx1 = bx0 + width
    rounded_rect(d, (bx0, y - pad[1], bx1, y + bh - pad[1]), 6, fill=bg)
    d.text((x, y), text, font=fnt, fill=fg)
    canvas.paste(cw)


def draw_roster(canvas, rect, rows):
    """Redraw a roster panel: dark cover + crisp rows (face, name, number)."""
    x0, y0, x1, y1 = [int(v) for v in rect]
    cw = canvas.convert("RGBA")
    d = ImageDraw.Draw(cw)
    rounded_rect(d, (x0, y0, x1, y1), 8, fill=(16, 20, 18, 235))
    row_h = (y1 - y0) // len(rows)
    for i, (name, num, slug) in enumerate(rows):
        ry = y0 + i * row_h
        slot = min(30, row_h - 8)
        img = face_image(slug, slot)
        cw.paste(img, (x0 + 10, ry + (row_h - slot) // 2), img)
        d.text((x0 + 44, ry + row_h // 2 - 6), name, font=font(12),
               fill=(228, 230, 220, 240))
        ns = str(num)
        d.text((x1 - 18 - text_w(d, ns, font(12, True)), ry + row_h // 2 - 6),
               ns, font=font(12, True), fill=(228, 230, 220, 240))
    canvas.paste(cw)


def draw_sample_log(canvas, xy, lines, size=12, fg=(205, 214, 228, 235)):
    """Draw crisp sample content lines (action log / chat)."""
    x, y = xy
    cw = canvas.convert("RGBA")
    d = ImageDraw.Draw(cw)
    for i, (txt, col) in enumerate(lines):
        d.text((x, y + i * (size + 7)), txt, font=font(size), fill=col or fg)
    canvas.paste(cw)


# ---------------------------------------------------------------- configs
# field_rect: (x, y, w, h) where the real field goes (1344x768 canvas)
# boxes: (side, team_color, label_color, {RESERVE:[slugs], KO:[...], INJURED:[...]})

ORC = (150, 190, 90)
DWARF = (110, 150, 200)
GOLD = (200, 164, 92)
SILVER = (150, 170, 205)

CONFIGS = {
    # C2: Qwen-Image 2.1 docking mockup — crisp AI text; real field pasted
    # into the central dock, PIL boxes, garbled log lines 3-4 fixed, CHAT
    # lines drawn, translucent card on the empty left dock.
    "C2": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/C_qds_docking_s22_00001_.png",
        "field_rect": (250, 115, 830, 505),
        "boxes": [
            ("top", GOLD, (216, 190, 130, 235),
             {"RESERVE": ["orc_blitzer", "orc_lineman", "orc_thrower"],
              "KO": ["black_orc", "renegade_orc"], "INJURED": [None]}, "HOME"),
            ("bottom", SILVER, (150, 175, 215, 235),
             {"RESERVE": ["dwarf_blitzer", "dwarf_lineman", "dwarf_runner"],
              "KO": ["chaos_dwarf_blocker"], "INJURED": [None, None]}, "AWAY"),
        ],
        "card": "left",
        "card_pos": (28, 150),
        "titles": [],
        "cover": [(1146, 152, 1330, 198)],
        "chat": [
            ((1163, 156), [("Dodge: 2, 1 - failed", None),
                          ("Move: Snurr 1 square", None)]),
            ((1150, 285), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # C: QDS docking (Z-Image round) — kept for comparison.
    "C": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/C_qds_docking_00002_.png",
        "field_rect": (168, 92, 1008, 580),
        "boxes": [
            ("top", GOLD, (216, 190, 130, 235),
             {"RESERVE": ["orc_blitzer", "orc_lineman", "orc_thrower"],
              "KO": ["black_orc", "renegade_orc"], "INJURED": [None]}, "HOME"),
            ("bottom", SILVER, (150, 175, 215, 235),
             {"RESERVE": ["dwarf_blitzer", "dwarf_lineman", "dwarf_runner"],
              "KO": ["chaos_dwarf_blocker"], "INJURED": [None, None]}, "AWAY"),
        ],
        # new design: left roster list is removed (empty dock area)
        "cover": [(14, 62, 204, 670)],
        "card": "left",
        "card_pos": (24, 150),
        "titles": [
            ("ACTION LOG", (1206, 66), 13, "center"),
            ("CHAT", (1206, 393), 13, "center"),
        ],
    },
    # B2: Qwen-Image 2.1 glass mockup — crisp AI text (ACTION LOG/CHAT/TD
    # already perfect); real field pasted in, PIL boxes on the end rows,
    # garbled chat lines 2-3 overwritten, translucent card on the LEFT edge.
    "B2": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/B_2d_glass_s11_00001_.png",
        "field_rect": (223, 149, 895, 492),
        "boxes": [
            ("top", GOLD, (216, 190, 130, 235),
             {"RESERVE": ["orc_blitzer", "orc_lineman", "orc_thrower"],
              "KO": ["black_orc", "renegade_orc"], "INJURED": [None]}, "HOME"),
            ("bottom", SILVER, (150, 175, 215, 235),
             {"RESERVE": ["dwarf_blitzer", "dwarf_lineman", "dwarf_runner"],
              "KO": ["chaos_dwarf_blocker"], "INJURED": [None, None]}, "AWAY"),
        ],
        "card": "left",
        "card_pos": (24, 185),
        "titles": [],
        "cover": [(1178, 260, 1335, 312)],  # mask the two garbled chat lines
        "chat": [
            ((1198, 262), [("gg wp", None), ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # B: 2D glass panels (Z-Image round) — kept for comparison.
    "B": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/B_2d_transparent_00002_.png",
        "field_rect": (330, 152, 720, 412),
        "boxes": [
            ("top", GOLD, (216, 190, 130, 235),
             {"RESERVE": ["orc_blitzer", "orc_lineman", "orc_thrower"],
              "KO": ["black_orc", "renegade_orc"], "INJURED": [None]}, "HOME"),
            ("bottom", SILVER, (150, 175, 215, 235),
             {"RESERVE": ["dwarf_blitzer", "dwarf_lineman", "dwarf_runner"],
              "KO": ["chaos_dwarf_blocker"], "INJURED": [None, None]}, "AWAY"),
        ],
        # new design: left roster list + old-style corner player card removed
        "cover": [(14, 26, 258, 330), (14, 606, 324, 754)],
        "card": "right",
        "card_pos": (1116, 240),
        "titles": [
            ("ACTION LOG", (1122, 62), 13),
            ("CHAT", (1112, 622), 13),
        ],
    },
    # D2: Qwen-Image 2.1 fantasy dashboard — parchment + token ring (boxes)
    # + header; real field on the parchment; left panel title fixed (it was
    # 'ACTION LOG' over the roster) + roster redrawn crisp; right log/chat
    # lines fixed.
    "D2": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/D_fantasy_dashboard_s33_00001_.png",
        "field_rect": (280, 170, 750, 370),
        "boxes": [],  # the AI token ring around the parchment stays
        "card": None,
        "cover": [(1168, 240, 1315, 442), (1150, 515, 1322, 590)],
        "titles": [
            ("ROSTER", (112, 119), 13, "center"),
        ],
        "roster": {
            "rect": (62, 142, 176, 578),
            "rows": [
                ("Gorker", 1, "orc_lineman"), ("Snurr", 2, "orc_thrower"),
                ("Thraka", 3, "orc_blitzer"), ("Muzzag", 4, "black_orc"),
                ("Grudg", 5, "renegade_orc"), ("Blashnik", 6, "orc_lineman"),
                ("Ughna", 7, "orc_thrower"), ("Ferkul", 8, "orc_blitzer"),
                ("Drokk", 9, "black_orc"), ("Snaag", 10, "orc_lineman"),
            ],
        },
        "chat": [
            ((1180, 254), [("Move: Gorker 2 squares", None),
                          ("Block: 5, 6, 3 - success", None),
                          ("Dodge: 2, 1 - failed", None),
                          ("Move: Snurr 1 square", None),
                          ("Blitz: Thrain → Gorker", (210, 130, 120, 235))]),
            ((1163, 524), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # D: fantasy dashboard (Z-Image round) — real field on the parchment; the
    # AI already drew a token ring around it (the boxes!).
    "D": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/D_fantasy_dashboard_00001_.png",
        "field_rect": (410, 192, 536, 292),
        "boxes": [],
        "card": None,  # left side is the roster in this variant
        "titles": [
            ("ACTION LOG", (1012, 140), 15),
            ("CHAT", (1012, 372), 15),
        ],
        "chat": [
            ((1018, 408), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # A33: Qwen-Image 2.1 BB3 3D broadcast, seed 55 (wider shot, card stats
    # already correct) — roster names + CHAT lines redrawn crisp.
    "A33": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/A3_bb3_3d_s55_00001_.png",
        "field_rect": None,
        "boxes": [],
        "cover": [(30, 70, 205, 310), (1136, 195, 1325, 258)],
        "roster": {
            "rect": (30, 70, 205, 310),
            "rows": [
                ("Gorker", 1, "orc_lineman"), ("Snurr", 2, "orc_thrower"),
                ("Thraka", 3, "orc_blitzer"), ("Muzzag", 4, "black_orc"),
                ("Grudg", 5, "renegade_orc"), ("Blashnik", 6, "orc_lineman"),
                ("Ughna", 7, "orc_thrower"), ("Ferkul", 8, "orc_blitzer"),
            ],
        },
        "chat": [
            ((1150, 199), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # A32: Qwen-Image 2.1 BB3 3D broadcast — scene + crisp HUD kept; roster
    # names redrawn crisp, player card stats fixed, garbled CHAT lines fixed.
    "A32": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/A3_bb3_3d_s44_00001_.png",
        "field_rect": None,
        "boxes": [],
        "cover": [(13, 62, 196, 320), (22, 544, 198, 590), (1168, 140, 1332, 196)],
        "roster": {
            "rect": (13, 62, 196, 320),
            "rows": [
                ("Gorker", 1, "orc_lineman"), ("Snurr", 2, "orc_thrower"),
                ("Thraka", 3, "orc_blitzer"), ("Muzzag", 4, "black_orc"),
                ("Grudg", 5, "renegade_orc"), ("Blashnik", 6, "orc_lineman"),
                ("Ughna", 7, "orc_thrower"), ("Ferkul", 8, "orc_blitzer"),
            ],
        },
        "chat": [
            ((32, 548), [("MA 1    ST 3    AG 2", (235, 240, 250, 240)),
                        ("PA 0    AV 0", (235, 240, 250, 240))]),
            ((1185, 146), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
    # A3: BB3 3D broadcast (Z-Image round) — NO field replacement (the 3D
    # scene is the point).
    "A3": {
        "in": "/home/saruman/ComfyUI-Shared/output/mockups/A3_bb3_full_00001_.png",
        "field_rect": None,
        "boxes": [],
        "cover": [(8, 606, 334, 756)],
        "chat_panel": (1085, 345, 1318, 545),
        "card": "left",
        "card_pos": (34, 150),
        "titles": [
            ("ROSTER", (48, 72), 13),
            ("ACTION LOG", (1105, 97), 14),
            ("CHAT", (1100, 358), 14),
        ],
        "chat": [
            ((1100, 392), [("gg wp", None), ("that block was insane", None),
                          ("next: blitz mid", (200, 170, 120, 230))]),
        ],
    },
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config", choices=list(CONFIGS))
    args = ap.parse_args()
    cfg = CONFIGS[args.config]

    canvas = Image.open(cfg["in"]).convert("RGBA")
    if canvas.size != (1344, 768):
        canvas = canvas.resize((1344, 768), Image.LANCZOS)

    # 1. real field (2D mockups only; A3 keeps the AI 3D field)
    if cfg.get("field_rect"):
        paste_field(canvas, cfg["field_rect"])

    # 2. boxes (on the field's end rows)
    fr = cfg.get("field_rect")
    for entry in cfg.get("boxes", []):
        side, tcol, lcol, zones = entry[0], entry[1], entry[2], entry[3]
        tag = entry[4] if len(entry) > 4 else ""
        draw_boxes(canvas, fr, side, tcol, lcol, zones, tag)

    # 3. cover removed panels (new design: no side roster, no corner card)
    if cfg.get("cover"):
        cover_rects(canvas, cfg["cover"])

    # 4b. optional standalone translucent panel (A3's CHAT)
    if cfg.get("chat_panel"):
        r = cfg["chat_panel"]
        cw = canvas.convert("RGBA")
        d = ImageDraw.Draw(cw)
        rounded_rect(d, r, 12, fill=(10, 14, 24, 200),
                     outline=(130, 165, 225, 100), width=2)
        canvas.paste(cw)

    # 4. translucent player card (on top of everything at the screen edge)
    if cfg.get("card"):
        fixture = json.loads(FIXTURE.read_text())
        draw_player_card(canvas, cfg["card"], fixture,
                         pos=cfg.get("card_pos"))

    # 5. crisp titles
    for t in cfg.get("titles", []):
        txt, xy, size = t[0], t[1], t[2]
        fix_title(canvas, xy, txt, size=size,
                  align=t[3] if len(t) > 3 else "left")

    # 6. sample chat lines
    for c in cfg.get("chat", []):
        xy, lines = c
        draw_sample_log(canvas, xy, lines)

    # 7. roster redraw (D2)
    if cfg.get("roster"):
        draw_roster(canvas, cfg["roster"]["rect"], cfg["roster"]["rows"])

    OUTDIR.mkdir(parents=True, exist_ok=True)
    out = OUTDIR / f"hybrid_{args.config}.png"
    canvas.convert("RGB").save(out, quality=95)
    print(out)


if __name__ == "__main__":
    main()
