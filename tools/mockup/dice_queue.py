#!/usr/bin/env python3
"""Dice sets for the FUMBBL Reborn client — Qwen-Image 2.1 Turbo (CPU).

Generates the two die sets used by the Mockup F action log:

  * BLOCK DICE (6 faces) — must look IDENTICAL to the official Games
    Workshop Blood Bowl block dice (GW is the only authority for the
    graphics — FUMBBL's own art does not count). Official modern symbol
    set (verified against GW sources, see memory bank round 5):
        1 = skull
        2 = skull inside a starburst
        3 = chunky push arrow with a lightning-bolt tail
        4 = the SAME chunky push arrow (there is NO single/double arrow)
        5 = "!" inside a starburst
        6 = starburst (POW)
  * D6 (6 faces) — red die with white round pips, values 1..6 (standard
    pip layout). Every roll in the action log is shown as the d6 face.

REPRODUCIBILITY / FUTURE COLOR SETS:
  Every job is fully described in the manifest written next to this file
  (dice/manifest.json): exact prompt, seed, size, style parameters. A
  future "pick your set" feature = re-run this script with different
  die_color / symbol_color values (same style template + same seeds) to
  produce a set that is identical in style but differently colored.
  Raw renders are kept under ComfyUI-Shared/output/mockups/dice_set_v1/.

Usage:
    python3 dice_queue.py                 # queue all 12 faces
    python3 dice_queue.py block           # only the 6 block faces
    python3 dice_queue.py d6              # only the 6 d6 faces
    python3 dice_queue.py status          # show pending/finished history
"""
import json
import os
import sys
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"
HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, "dice", "manifest.json")

# ---------------------------------------------------------------------------
# Style template — parameterized so future sets differ ONLY in colors.
# The die face fills the whole frame; post-processing (PIL) crops/masks it
# into a rounded square asset.
# ---------------------------------------------------------------------------
STYLE = (
    "A single square game die face, perfectly flat top-down view, the die "
    "face filling the ENTIRE frame edge to edge as a perfect square with "
    "slightly rounded corners. The die background is a solid {die_color}. "
    "Flat clean vector game-art style, crisp uniform edges, no gradients, "
    "no bevel, no 3D perspective, no texture noise, no shadow. "
    "In the exact center of the die face: a white circular badge about 70% "
    "of the face size, whose rim has many small irregular jagged spikes "
    "around the whole circle (a round white frame with a torn, spiky edge, "
    "like the official Games Workshop Blood Bowl block dice badge). "
    "Inside the white badge, perfectly centered, one bold {symbol_color} "
    "symbol, well inside the badge edge: {symbol}. "
    "Nothing else: no text, no numbers, no letters, no watermark, no "
    "border frame around the square."
)

# The d6 has NO white badge: plain white round pips directly on the die.
STYLE_D6 = (
    "A single square game die face, perfectly flat top-down view, the die "
    "face filling the ENTIRE frame edge to edge as a perfect square with "
    "slightly rounded corners. The die background is a solid {die_color}. "
    "Flat clean vector game-art style, crisp uniform edges, no gradients, "
    "no bevel, no 3D perspective, no texture noise, no shadow, no badge. "
    "On the die face, large round {d6_dot_color} dots in this arrangement: "
    "{symbol}. The dots are smooth perfect circles, evenly sized, evenly "
    "spaced, clearly inside the edges, high contrast against the die. "
    "Nothing else: no text, no numbers, no letters, no symbols, no "
    "watermark, no border frame around the square."
)

SET_PARAMS = {
    # Official GW schema (photo ref, MeepleLike 2016): colored die face +
    # white circular badge with a jagged spiky rim + symbol INSIDE the badge
    # in the DIE color. Our set: red die, white badge, red symbol.
    "die_color": "crimson red (like #B02020)",
    "symbol_color": "the same solid crimson red as the die background",
    # The d6 has NO white badge: its pips are pure white directly on the die.
    "d6_dot_color": "pure white",
}

# Official GW block-die symbols (verified on the official dice set photo +
# user correction: the 5 is an EXCLAMATION MARK inside the burst, not an X).
BLOCK_SYMBOLS = {
    1: "a bold skull icon in a clean game-logo style: rounded cranium, two "
       "large round eye sockets, a small nasal triangle, and a lower jaw "
       "with visible square teeth, solid flat fill",
    # Round 8b (user): the BOTH-DOWN face must be clearly different from the
    # plain skull — the burst is BIG (about the same size as the skull) and
    # sits clearly apart, upper-right.
    2: "a bold skull icon (rounded cranium, two large round eye sockets, "
       "nasal triangle, jaw with square teeth) on the lower-left of the "
       "badge, and a BIG sharp spiky explosion burst on the upper-right, "
       "about the same size as the skull, clearly separated from it, as "
       "if the skull just exploded — the two shapes are distinct and "
       "both easy to read, solid flat fill",
    # Round 8b (user): the push arrow is a SIMPLE bold arrow — NO lightning
    # bolt. Straight clean tail.
    3: "one single simple bold push arrow pointing diagonally up-right at "
       "45 degrees: a hollow double-line outlined triangular arrowhead and "
       "a straight rectangular shaft with a clean diagonal cut at the "
       "tail, no lightning, no zigzag, just a plain chunky arrow",
    4: "one single simple bold push arrow pointing diagonally up-right at "
       "45 degrees: a hollow double-line outlined triangular arrowhead and "
       "a straight rectangular shaft with a clean diagonal cut at the "
       "tail, no lightning, no zigzag, just a plain chunky arrow",
    5: "a bold exclamation mark (a thick bar with a separated square dot "
       "below it) tilted slightly, surrounded by a sharp spiky starburst "
       "explosion ring, solid flat fill",
    6: "a bold spiky starburst explosion symbol with 8 to 10 rough "
       "uneven points, solid flat fill, like a comic-book POW explosion",
}

D6_SYMBOLS = {
    1: "one single large round dot",
    2: "two round dots, one at upper-left and one at lower-right",
    3: "three round dots arranged on a diagonal from upper-left to "
       "lower-right",
    4: "four round dots, one in each of the four corners",
    5: "five round dots, one in each of the four corners plus one in the "
       "center",
    6: "six round dots arranged in two vertical columns of three",
}

SEEDS = {"block": {1: 101, 2: 102, 3: 103, 4: 104, 5: 105, 6: 106},
         "d6": {1: 201, 2: 202, 3: 203, 4: 204, 5: 205, 6: 206}}

SIZE = 512  # dice render ~24px in the log; 512 is a safe Qwen-Image minimum


def prompt_for(kind, value):
    symbol = (BLOCK_SYMBOLS if kind == "block" else D6_SYMBOLS)[value]
    if kind == "block":
        return STYLE.format(die_color=SET_PARAMS["die_color"],
                            symbol_color=SET_PARAMS["symbol_color"],
                            symbol=symbol)
    return STYLE_D6.format(die_color=SET_PARAMS["die_color"],
                           d6_dot_color=SET_PARAMS["d6_dot_color"],
                           symbol=symbol)


def build_workflow(kind, value, seed):
    prompt = prompt_for(kind, value)
    prefix = f"mockups/dice_set_v1/{kind}_{value}"
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
              "inputs": {"width": SIZE, "height": SIZE, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["6", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 1.0}},
        "8": {"class_type": "VAEDecode",
              "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"images": ["8", 0], "filename_prefix": prefix}},
    }


def write_manifest(jobs):
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    manifest = {
        "set": "v1",
        "note": "Dice sets for the FUMBBL Reborn action log. A new color "
                "set = same style/seeds, different die_color/symbol_color.",
        "style_template": STYLE,
        "style_template_d6": STYLE_D6,
        "d6_dot_color": SET_PARAMS["d6_dot_color"],
        "params": SET_PARAMS,
        "size": SIZE,
        "sampler": {"steps": 8, "cfg": 1.0, "sampler_name": "euler",
                    "scheduler": "simple", "denoise": 1.0},
        "models": {
            "unet": "qwen_image_2.1_turbo_int8_convrot.safetensors",
            "clip": "qwen3vl_8b_int8_convrot.safetensors",
            "vae": "qwen_image_2.1_vae_bf16.safetensors",
        },
        "jobs": {f"{k}_{v}": {"seed": SEEDS[k][v], "prompt": prompt_for(k, v)}
                 for k, v in jobs},
    }
    with open(MANIFEST, "w") as f:
        json.dump(manifest, f, indent=2)
    print("manifest ->", MANIFEST)


def queue(kind, value):
    seed = SEEDS[kind][value]
    body = json.dumps({"prompt": build_workflow(kind, value, seed)}).encode()
    req = urllib.request.Request(API + "/prompt", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        d = json.load(r)
    print(f"queued {kind}_{value} seed={seed} -> {d.get('prompt_id')}")


def main():
    # Usage: dice_queue.py [all|block|d6|block:2,3,4] [--seed-bump N]
    sel = "all"
    seed_bump = 0
    args = [a for a in sys.argv[1:]]
    i = 0
    while i < len(args):
        if args[i] == "--seed-bump" and i + 1 < len(args):
            seed_bump = int(args[i + 1])
            i += 2
            continue
        sel = args[i]
        i += 1
    if sel == "status":
        try:
            with urllib.request.urlopen(API + "/queue", timeout=10) as r:
                q = json.load(r)
            print("pending:", len(q.get("queue_pending", [])),
                  "| running:", len(q.get("queue_running", [])))
        except urllib.error.URLError:
            print("ComfyUI not reachable at", API)
        return
    jobs = []
    if sel == "all":
        for k in ("block", "d6"):
            jobs.extend((k, v) for v in range(1, 7))
    elif ":" in sel:
        k, vs = sel.split(":", 1)
        for v in vs.split(","):
            if v.strip():
                jobs.append((k, int(v)))
    else:
        jobs.extend((sel, v) for v in range(1, 7))
    write_manifest(jobs)
    global SEEDS
    if seed_bump:
        SEEDS = {k: {v: sd + seed_bump for v, sd in vals.items()}
                 for k, vals in SEEDS.items()}
    for k, v in jobs:
        queue(k, v)
    print(f"{len(jobs)} dice job(s) queued (seed_bump={seed_bump})")


if __name__ == "__main__":
    main()
