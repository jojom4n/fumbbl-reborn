#!/usr/bin/env python3
"""Round 13 — Qwen-Image 2.1 Turbo (CPU) generations.

1. BLOCK DICE v2 (6 faces, 512x512) — regenerated to match the OFFICIAL
   Games Workshop block dice (red set) PIXEL-FOR-PIXEL in design. Ground
   truth studied from official-set photos (MeepleLikeUs 2016-11-28 shoot +
   blood-bowl-miniatures.de 2019 red set), crops in /tmp/gw_dice/:
     * die face = solid flat crimson red, rounded square, edge to edge
     * white circular BADGE with an irregular spiky torn rim (12-16 uneven
       spikes, varying length — organic explosion-sticker look)
     * the symbol sits INSIDE the badge in the DIE COLOR (red), large
   Faces:
     1 = filled skull (rounded cranium, slanted angry eyes, triangle nose,
         4 square teeth)
     2 = skull lower-left + spiky burst upper-right (same size as skull,
         red spikes with white core)
     3/4 = thick HOLLOW push arrow at 45 deg up-right (red double-line
         outline, white interior)
     5 = red "!" (tilted bar + square dot) inside a spiky burst ring
     6 = red spiky star RING with hollow white center (POW)
   (The round-12 PIL restyle that flattened the dice — badge removed,
   glyphs turned white — is OBSOLETE: the assets come straight from these
   renders, official design intact.)
2. FIELD v3 (2688x1536) — GRASS ONLY. No grid, no TD zones, no TOUCHDOWN
   text, no center line: round 13 moves ALL of that to vector code
   (crisp at any zoom / HiDPI). The texture must be pure grass with
   subtle mottling so the code-drawn mowing stripes carry the contrast.

Usage:
    python3 r13_queue.py dice      # queue the 6 block faces
    python3 r13_queue.py field     # queue the grass texture
    python3 r13_queue.py all
    python3 r13_queue.py status
"""
import json
import sys
import time
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"
OUT_DIR = "/home/saruman/ComfyUI-Shared/output/r13"

DIE_RED = "a solid flat crimson red (around #B02020), completely uniform, no gradient, no bevel, no 3D perspective, no texture noise, no shadow"

BLOCK_STYLE = (
    "A single square game die face, perfectly flat top-down view, the die "
    "face filling the ENTIRE frame edge to edge as a perfect square with "
    "slightly rounded corners. The die background is {die}. Flat clean 2D "
    "vector game-art style, crisp uniform edges, high contrast. "
    "In the exact center of the die face: a WHITE badge, a circular disc "
    "about 72% of the face width, whose edge is a SPIKY BURST: 11 to 12 "
    "SHORT, POINTED, ANGULAR triangular spikes of VARYING length (some "
    "clearly longer, some shorter, uneven) radiating outward — each spike "
    "is a sharp triangle with a slightly soft tip but clearly ANGULAR and "
    "POINTED, like a comic-book explosion star or a torn-paper burst. The "
    "edge must be SPIKY and angular, NOT a smooth scalloped/flower edge, "
    "NOT a wavy border, NOT rounded bumps. The interior of the badge is "
    "solid clean white. "
    "Inside the badge, perfectly centered, LARGE (filling about 75% of "
    "the badge), in solid crimson red (the die color): {symbol}. "
    "Nothing else: no text, no numbers, no letters, no watermark, no "
    "border frame around the square, no second badge."
)

BLOCK_SYMBOLS = {
    1: ("a bold filled skull in a clean fantasy game-logo style: a rounded "
        "domed cranium, two large slanted angular ANGRY eye sockets, a "
        "small inverted-triangle nasal cavity, and a lower jaw with four "
        "square teeth — the eye sockets, nose and tooth gaps are white "
        "cutouts (the badge color), the skull itself solid flat red"),
    2: ("a bold filled red skull on the LOWER-LEFT (rounded domed cranium, "
        "slanted angry eye sockets, triangular nose, jaw with four square "
        "teeth, white cutouts) AND a sharp spiky explosion starburst on "
        "the UPPER-RIGHT about the SAME size as the skull — the burst is "
        "solid red uneven spikes with a small white core in its middle; "
        "the skull and the burst slightly overlap at their edges but both "
        "stay clearly readable, like an exploding skull"),
    3: ("one single thick push arrow pointing diagonally UP-RIGHT at 45 "
        "degrees, drawn as a HOLLOW DOUBLE-LINE OUTLINE (red outline "
        "lines, white interior like the badge): a triangular arrowhead "
        "with a double-line outline and a chunky rectangular shaft with a "
        "clean diagonal cut at the tail — a bold game-UI style arrow"),
    4: ("one single thick push arrow pointing diagonally DOWN-LEFT at 45 "
        "degrees, drawn as a HOLLOW DOUBLE-LINE OUTLINE (red outline "
        "lines, white interior like the badge): a triangular arrowhead "
        "with a double-line outline and a chunky rectangular shaft with a "
        "clean diagonal cut at the tail — a bold game-UI style arrow"),
    5: ("a bold exclamation mark in solid red (a thick slightly tilted "
        "vertical bar with a separated square dot below it) surrounded by "
        "a spiky starburst ring of red spikes radiating outward that "
        "nearly reaches the badge rim, the spikes uneven in length"),
    6: ("a SOLID star in solid flat red, perfectly centered, with EXACTLY "
        "TWELVE (12) sharp points — count them, there must be 12 — each "
        "point a thin elongated DIAMOND (like a compass star or a "
        "comic-book explosion burst), the whole star SOLID flat red with "
        "sharp clean points and no hollow center, filling about 80% of "
        "the badge"),
}

DICE_SEEDS = {1: 9201, 2: 9202, 3: 9203, 4: 9204, 5: 9205, 6: 9206}

FIELD_GRASS_POS = (
    "Spectacular AAA fantasy sports video game field texture, perfectly "
    "flat top-down view filling the ENTIRE frame edge to edge with no "
    "border, no margin and no frame. Rich natural green grass with visible "
    "individual grass blades in fine detail, a soft even daylight look, "
    "subtle organic mottling across the surface: gentle patches of "
    "slightly lighter and slightly darker green blending into each other, "
    "a few faint worn areas and very light scuff marks in the middle, dry "
    "healthy grass. STRICT: no grid lines, no white lines of any kind, no "
    "stripes, no center line, no numbers, no text, no letters, no red "
    "zones, no banners, no touchdowns, no players, no ball, no logos, no "
    "emblems, no watermarks, no stadium, no crowd, no rain, no puddles, no "
    "wet spots, no shadows of objects, no vignette — only a flat uniform "
    "top-down grass field texture."
)


def build_workflow(w, h, pos, prefix, seed):
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
              "inputs": {"text": pos, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": w, "height": h, "batch_size": 1}},
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


def queue(wf):
    req = urllib.request.Request(
        API + "/prompt", data=json.dumps({"prompt": wf}).encode(),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        d = json.load(r)
    if d.get("node_errors"):
        raise RuntimeError(f"node errors: {d['node_errors']}")
    print(f"queued -> {d['prompt_id']}", flush=True)
    return d["prompt_id"]


def queue_dice():
    for v in range(1, 7):
        pos = BLOCK_STYLE.format(die=DIE_RED, symbol=BLOCK_SYMBOLS[v])
        queue(build_workflow(512, 512, pos, f"r13/block_{v}", DICE_SEEDS[v]))
        print(f"  block_{v} seed={DICE_SEEDS[v]}", flush=True)


def queue_field():
    queue(build_workflow(2688, 1536, FIELD_GRASS_POS, "r13/field_grass_v3", 20261027))


def status():
    try:
        with urllib.request.urlopen(API + "/queue", timeout=10) as r:
            q = json.load(r)
        print("pending:", len(q.get("queue_pending", [])),
              "| running:", len(q.get("queue_running", [])))
    except urllib.error.URLError:
        print("ComfyUI not reachable at", API)


if __name__ == "__main__":
    job = sys.argv[1] if len(sys.argv) > 1 else "status"
    if job == "dice":
        queue_dice()
    elif job == "field":
        queue_field()
    elif job == "all":
        queue_dice()
        queue_field()
    else:
        status()
