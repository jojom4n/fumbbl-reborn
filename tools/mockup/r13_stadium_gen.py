#!/usr/bin/env python3
"""Round 13 — Qwen-Image top-down FANTASY STADIUM (stadium ONLY, no field).

The stadium is a SEPARATE, interchangeable asset (the field is its own
asset, drawn on top). Layout is strict so a companion MANIFEST can align the
players' faces to the dugouts of ANY stadium in the future:

  - CENTER  : a large, empty, uniform dark-stone FIELD PIT (no grass/lines).
  - TOP     : two symmetric DUGOUT pavilions (home LEFT, away RIGHT), each a
              neat row of EMPTY benches, no text/numbers/symbols.
  - AROUND  : tiered stands packed with a beautiful, cohesive medieval crowd.

The code reads <stadium>.json (normalized fieldPit / dugoutHome / dugoutAway
rects) to place the field + player faces — the alignment system is
data-driven, so new stadiums just ship their own manifest.

Usage:
    python3 r13_stadium_gen.py queue
    python3 r13_stadium_gen.py status
"""
import json
import sys
import urllib.request

API = "http://127.0.0.1:8188"

# Round 13 turn 4 (user): the dark-fantasy arena was REJECTED — the new style
# is LIGHT and CLEAN, inspired by the user's reference (a bright, modern,
# flat game-art top-down stadium): wood + warm harmonious tones, beautiful
# realistic spectators, no dark fantasy, no torches. The image stays
# LANDSCAPE (1920x1088, aspect ~1.765 ~= the field's 1.733) so the central
# FIELD PIT comes out landscape (the real 26x15 field fits it); top band
# holds the two DUGOUT pavilions (home LEFT, away RIGHT); all other bands
# = packed beautiful stands.
STADIUM_POS = (
    "Beautiful modern video-game STADIUM seen from DIRECTLY ABOVE (perfectly "
    "flat top-down 2D, zero perspective, no horizon, straight bird's-eye "
    "view). This is a WIDE landscape image (much wider than tall).\n"
    "LAYOUT, strictly followed (sizes are % of the image, the image is 5:3):\n"
    "- CENTER FIELD PIT: one large, perfectly RECTANGULAR, completely EMPTY "
    "floor — uniform matte warm DARK-TAN packed-dirt/stone, one single flat "
    "color, NO grass, NO field, NO lines, NO markings, NO objects, clean "
    "straight edges, bordered by a thin light-wood frame. CRITICAL: the pit "
    "must be EXACTLY 1.73 times WIDER than tall (the ratio of a Blood Bowl "
    "field). It spans from 12% to 88% of the image WIDTH (76% of the width) "
    "and from 12% to 84% of the image HEIGHT (72% of the height) — measure "
    "this ratio carefully, it must be about 1.73:1.\n"
    "- TOP BAND (the top ~11% of the height, directly above the pit): two "
    "SYMMETRIC DUGOUT pavilions of LIGHT warm wood (oak/beech) — the LEFT "
    "pavilion spans ~12%..48% of the width, the RIGHT pavilion spans "
    "~52%..88% of the width — each a long, clean horizontal pavilion with "
    "EMPTY wooden bench seats (team benches) facing the pit, neat plank "
    "floor, a small light gap in the middle, the pavilions sitting CLOSE to "
    "the pit (their bottom edge almost touches the pit frame). Absolutely NO "
    "text, NO numbers, NO symbols, NO letters.\n"
    "- BOTTOM band (the bottom ~16% of the height) and the left/right strips "
    "(the ~12% on each side of the pit): tiered stadium STANDS rising away "
    "from the field, PACKED with a DENSE, BEAUTIFUL, REALISTIC crowd seen "
    "from above — thousands of tiny heads in a cohesive, harmonious, "
    "painterly mosaic (warm earthy tones: rust, ochre, cream, forest green, "
    "navy, soft red — like a real packed stadium), elegant and dense, NOT a "
    "chaotic mess; light-wood aisle dividers and low walls separate the "
    "blocks; the whole thing reads as a GORGEOUS aerial crowd.\n"
    "STYLE: LIGHT, CLEAN, bright modern flat game-art (like a premium sports "
    "video game), warm daylight, wood + cream + soft harmonious colors, "
    "crisp clean 2D top-down art, soft even lighting, NO dark fantasy, NO "
    "medieval, NO stone, NO torches, NO lanterns.\n"
    "STRICT: no text, no logos, no watermark, no camera, no letters, no "
    "numbers, no symbols, no grass, no pitch, no players, no torches."
)


def build(w, h, pos, prefix, seed):
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


def main():
    job = sys.argv[1] if len(sys.argv) > 1 else "status"
    if job == "queue":
        # 1920x1152 (5:3): tall enough that the pit can be 1.73:1 (the
        # field's ratio) — pit 76%w x 72%h = 1459x830 = 1.758 ~= 1.733 —
        # while still leaving the top dugout band + bottom stands. A few
        # seeds to pick the best. (stadium3 = turn-4 light/clean style,
        # corrected pit ratio.)
        for seed in (43001, 43002, 43003):
            queue(build(1920, 1152, STADIUM_POS, "r13/stadium3", seed))
            print(f"  stadium 1920x1152 seed={seed}", flush=True)
    else:
        import urllib.error
        try:
            with urllib.request.urlopen(API + "/queue", timeout=10) as r:
                q = json.load(r)
            print("pending:", len(q.get("queue_pending", [])),
                  "| running:", len(q.get("queue_running", [])))
        except urllib.error.URLError:
            print("ComfyUI not reachable at", API)


if __name__ == "__main__":
    main()
