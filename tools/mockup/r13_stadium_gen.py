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

# The IMAGE is LANDSCAPE (1920x1088, aspect ~1.765 ~= the field's 1.733) so
# the central FIELD PIT that fills most of it comes out LANDSCAPE (the real
# 26x15 field, square cells, fits it with no big margins). Top band holds the
# two DUGOUT pavilions (home LEFT, away RIGHT) close to the pit; bottom band +
# side strips = packed stands.
STADIUM_POS = (
    "Exquisite AAA fantasy video-game STADIUM seen from DIRECTLY ABOVE "
    "(perfectly flat top-down 2D, zero perspective, no horizon, straight "
    "bird's-eye view). This is a WIDE landscape image (much wider than tall).\n"
    "LAYOUT, strictly followed (sizes are % of the image):\n"
    "- CENTER FIELD PIT: one large, perfectly RECTANGULAR, completely EMPTY "
    "dark stone floor — uniform matte charcoal flagstone, NO grass, NO field, "
    "NO lines, NO markings, NO objects, clean straight edges. It is a WIDE "
    "LANDSCAPE rectangle (about 1.7 times WIDER than tall). It spans from ~6% "
    "to ~94% of the image WIDTH and from ~22% to ~86% of the image HEIGHT, "
    "leaving only a thin strip of stands on the far left and far right.\n"
    "- TOP BAND (the top ~20% of the height, directly above the pit): two "
    "SYMMETRIC medieval DUGOUT pavilions of dark oak and grey stone — the LEFT "
    "pavilion spans ~6%..46% of the width, the RIGHT pavilion spans ~54%..94% "
    "of the width — each a long horizontal row of a few rows of EMPTY wooden "
    "bench seats (team benches) facing the pit, a small stone gap in the "
    "middle, the pavilions sitting CLOSE to the pit. Absolutely NO text, NO "
    "numbers, NO symbols, NO letters.\n"
    "- BOTTOM band and the thin left/right strips: tiered stadium STANDS "
    "rising away from the field, PACKED with a dense, beautiful medieval "
    "crowd seen from above — a rich, cohesive, painterly mosaic of thousands "
    "of tiny fans in tunics and cloaks, warm HARMONIOUS palette (deep reds, "
    "ochres, browns, forest greens, muted blues), elegant and dense, NOT a "
    "chaotic mess of colors; stone aisles divide the blocks; a few team "
    "banners and pennants.\n"
    "STYLE: dark-fantasy medieval arena, warm torchlight, weathered stone + "
    "oak + iron, a few corner watchtowers with blue slate roofs, rich fine "
    "detail, crisp clean 2D top-down game art, soft even lighting.\n"
    "STRICT: no text, no logos, no watermark, no camera, no letters, no "
    "numbers, no symbols, no grass, no pitch, no players."
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
        # 1920x1088 = 1.765 aspect ~= the field's 1.733, so the pit that fills
        # the image comes out landscape. A few seeds to pick the best.
        for seed in (41001, 41002, 41003):
            queue(build(1920, 1088, STADIUM_POS, "r13/stadium", seed))
            print(f"  stadium 1920x1088 seed={seed}", flush=True)
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
