#!/usr/bin/env python3
"""Mockup queue for Qwen-Image 2.1 Turbo (official Comfy-Org int8 recipe).

Prompts describe OUR interface precisely (as we are thinking it):
  - ACTION LOG (all actions, not just dice) + CHAT  [core panels]
  - box rows (reserve/KO/injured) along top & bottom edges of the field
  - NO side roster lists (B, C) / roster kept (D)
  - strongly translucent player card on a screen EDGE (A3)
  - exact 26x15 blood bowl grid (never soccer)
Text-precision instructions included; titles are exact strings.

Usage: python3 qwen21_queue.py [B|C|D|A3|all]
"""
import json
import shutil
import sys
import time
import urllib.request

API = "http://127.0.0.1:8188"
OUT = "/home/saruman/ComfyUI-Shared/output/mockups"

FIELD = (
    "The game field is a flat top-down green grid board exactly 26 squares wide "
    "and 15 squares tall (a 26x15 grid with thin light grid lines and two end "
    "zones marked 'TD' at the left and right short ends). Players are small flat "
    "square tokens with a tiny portrait and a number: green orc tokens for the "
    "home team in a diagonal V formation on the left half, grey dwarf tokens for "
    "the away team in a diagonal V on the right half, one small brown ball near "
    "the center."
)

TEXT_RULES = (
    "All UI text must be perfectly crisp, correctly spelled, legible English in "
    "a clean sans-serif font; the panel titles must read exactly 'ACTION LOG' "
    "and 'CHAT' where present. No gibberish, no misspellings, no fake words."
)

NOT_FOOTBALL = (
    "Blood Bowl is a fantasy grid board game (NOT soccer, NOT American football: "
    "no goalposts, no soccer pitch markings, no round ball, no outfield lines)."
)

PROMPTS = {
    "B_2d_glass": (
        "Professional screenshot of a desktop esports client for Blood Bowl. "
        + NOT_FOOTBALL + " "
        + FIELD + " "
        "Full-screen 2D field view: the green 26x15 grid field fills most of the "
        "screen, dark forest stadium ambience around it, the field framed by a "
        "subtle glowing glass enclosure. Modern dark UI: semi-transparent dark "
        "glass panels with thin glowing blue-grey borders and crisp white text. "
        "Top bar: two team crests (green orc, grey dwarf), score '10 : 0', small "
        "re-rolls and fans meters. Right edge: a vertical panel titled 'ACTION "
        "LOG' with a short list of game actions, each line with a small dice "
        "icon (lines like 'Move: Gorker 2 squares', 'Block: 5, 6, 3 - success', "
        "'Dodge: 2, 1 - failed'); below it a panel titled 'CHAT' with three "
        "short chat messages. Left edge: empty, no panels, no roster. Along the "
        "top edge of the field a row of six small circular player slots (three "
        "filled with orc faces, three empty rings) and along the bottom edge a "
        "matching row with dwarf faces. " + TEXT_RULES
    ),
    "C_qds_docking": (
        "Professional screenshot of a desktop client for Blood Bowl. "
        + NOT_FOOTBALL + " "
        "QDS-style docking layout: several clearly separated floating panels "
        "with visible gaps, over a dark background with jungle fantasy stadium "
        "ambience. Center: a large dock containing the game field — "
        + FIELD + " "
        "Right side: a dock with two stacked panels — top titled 'ACTION LOG' "
        "(short list of actions with small dice icons, lines like 'Move: Gorker "
        "2 squares', 'Block: 5, 6, 3 - success'), bottom titled 'CHAT' (three "
        "short messages). Left side: an empty dark dock (no panels, no roster). "
        "Bottom: a thin dock with a row of five square action buttons with "
        "simple icons. Top: a slim bar with two team crests (green orc, grey "
        "dwarf) and score '10 : 0'. Along the top edge of the field a row of "
        "six small circular player slots (orc faces + empty rings) and along "
        "the bottom edge a matching row (dwarf faces). Semi-transparent dark "
        "glass panels, thin glowing borders. " + TEXT_RULES
    ),
    "D_fantasy_dashboard": (
        "Professional screenshot of a fantasy-styled desktop client for Blood "
        "Bowl. " + NOT_FOOTBALL + " Medieval fantasy dashboard: ornate dark "
        "stone and wood frame, torch-lit stadium with banners in the "
        "background. Center: an aged parchment panel containing the game field "
        "— " + FIELD + " A ring of small circular portrait tokens surrounds the "
        "parchment (team boxes). Top header: ornate bar with two team crests, "
        "big score '10 : 0', a fan factor meter. Left panel: team roster list "
        "(10 rows, small portrait + name + number). Right side: top panel "
        "titled 'ACTION LOG' with a small dice-result grid and short action "
        "lines; below it a panel titled 'CHAT' with three short messages. "
        "Bottom: a row of seven square action buttons with fantasy icons. "
        + TEXT_RULES
    ),
    "A3_bb3_3d": (
        "Professional in-game screenshot of a 3D Blood Bowl esports title. "
        + NOT_FOOTBALL + " Broadcast sports camera view from the sideline, "
        "slightly elevated: a 3D grass field with a visible white grid of "
        "exactly 26 by 15 squares, two teams of 3D fantasy players (muscular "
        "green orcs in red armor vs short grey bearded dwarves in blue armor) "
        "in diagonal formations, a small brown ball, a referee in a white "
        "shirt at the sideline, packed crowd in fantasy stadium stands with "
        "banners, night floodlights. Translucent broadcast HUD over the "
        "scene: top center scoreboard with two team crests and score '1 : 0'; "
        "left edge a translucent panel titled 'ROSTER' with 8 rows (small "
        "portrait + name + number); right edge top a translucent panel titled "
        "'ACTION LOG' with colored short action lines ('Block: 5, 6, 3 - "
        "success', 'Move: Thrain 2 squares'); below it a translucent panel "
        "titled 'CHAT' with three short messages; bottom center a bar of six "
        "square action buttons with glowing icons; on the left edge a "
        "strongly translucent player card with a large orc portrait, name "
        "'Gorker', stats 'MA 1  ST 3  AG 2  PA 0  AV 0' and skill chips 'BLOCK' "
        "and 'DODGE'. A broadcast TV camera rig visible in the bottom right "
        "corner. " + TEXT_RULES + " Cinematic game render, 16:9."
    ),

    "E_final": (
        "Professional screenshot of a desktop client for Blood Bowl. "
        + NOT_FOOTBALL + " Dark fantasy theme: very dark medieval stadium "
        "ambience visible only at the outer edges; NO decorative frames, "
        "borders or ornamental rings around the field — the field itself "
        "fills nearly the whole screen. "
        "Top header bar, dark and minimal: in the exact center the two team "
        "names and score in bold white 'DARK ORCS 2 - 1 SYLVAN ELVES'; "
        "center-right a small 'TURN 6' label and a small 'WEATHER: RAIN' "
        "label with a cloud icon; far left of the header a small group of "
        "three gold coin icons with label 'RE-ROLLS 2/3' (player 2 side); "
        "far right of the header a matching group 'RE-ROLLS 1/3' (player 1 "
        "side). "
        "Main area: a flat top-down green grid field, exactly 26 squares "
        "wide by 15 squares tall, thin light grid lines, two end zones "
        "marked 'TD' at the left and right short ends, slightly zoomed so "
        "the outer field edge is just cropped by the screen. Players are "
        "round circular portrait tokens: small painted face medallions with "
        "a number, green ring for the orc team (left half, V formation), "
        "gold ring for the elf team (right half, V formation); one active "
        "orc token highlighted with a red glow; two small white skull "
        "markers; a small brown ball near the center; dashed white "
        "movement arrows from the active token labeled 'Blitz' and 'Dodge "
        "83%'. Around the active token a right-click radial action menu: a "
        "ring of six small dark circular buttons with simple white icons "
        "(up arrow, shield, lightning bolt, curved pass arrow, two hands, "
        "crossed swords) evenly spaced on a circle around the token. Thin "
        "plain vertical strips hugging the left and right edges of the "
        "field: minimal dark slots, a short vertical row of six tiny square "
        "player slots each side (reserve / knocked out / injured boxes), "
        "flat and non-decorative. "
        "Bottom bar, dark, three zones: bottom-left a panel titled 'ACTION "
        "LOG' with short crisp lines 'Move: Gorker 2 squares', 'Block: 5, 6, "
        "3 - success', 'Dodge: 2, 1 - failed'; bottom-center a player card: "
        "large circular orc portrait, bold name 'Gorker', subtitle 'Orc "
        "Blitz #5', a stat row 'MA 6   ST 4   AG 2+   PA 3+   AV 10+', three "
        "small skill chips labeled 'Block', 'Mighty Blow', 'Tackle'; "
        "bottom-right a panel titled 'CHAT' with three short messages. "
        + TEXT_RULES
    ),
}

SEEDS = {"B_2d_glass": [11], "C_qds_docking": [22], "D_fantasy_dashboard": [33],
         "A3_bb3_3d": [44, 55], "E_final": [61, 62]}


def build_workflow(prompt, seed):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "4": {"class_type": "CLIPTextEncode",
              "inputs": {"text": prompt, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": 1344, "height": 768, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["6", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 1.0}},
        "8": {"class_type": "VAEDecode",
              "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"images": ["8", 0],
                         "filename_prefix": f"mockups/{name_q(seed)}"}},
    }


def name_q(seed):
    for n, ss in SEEDS.items():
        if seed in ss:
            return f"{n}_s{seed}"
    return f"unknown_{seed}"


def queue(prompt, seed):
    body = json.dumps({"prompt": build_workflow(prompt, seed)}).encode()
    req = urllib.request.Request(API + "/prompt", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        d = json.load(r)
    print("queued", d.get("prompt_id"), "for seed", seed)
    return d.get("prompt_id")


def main():
    sel = sys.argv[1] if len(sys.argv) > 1 else "all"
    jobs = [(n, s) for n, ss in PROMPTS.items() for s in SEEDS[n]
            if sel in ("all", n)]
    for n, s in jobs:
        queue(PROMPTS[n], s)
    print(f"{len(jobs)} jobs queued")


if __name__ == "__main__":
    main()
