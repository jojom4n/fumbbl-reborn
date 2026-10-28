#!/usr/bin/env python3
"""Round 13 — Qwen-Image top-down FANTASY STADIUM STANDS backdrop.

Fills the current grey stadium apron: packed medieval spectator stands
(top-down) around the field, inspired by Blood Bowl 3 stadiums. Rendered
STANDS-ONLY (no central field/grass) so it can be used as a full-widget
backdrop behind the field scene without any alignment issues (the field is
drawn on top and covers the middle).

Usage:
    python3 r13_stands.py queue
    python3 r13_stands.py status
"""
import json
import sys
import urllib.request

API = "http://127.0.0.1:8188"


STANDS_POS = (
    "Spectacular AAA fantasy video game STADIUM seen from DIRECTLY ABOVE "
    "(perfectly top-down, flat 2D, no perspective, no horizon). The frame is "
    "filled ENTIRELY with packed spectator STANDS and crowd — no field, no "
    "grass, no pitch, no center. Medieval Blood-Bowl-style stadium bowl: "
    "tiered curved seating rings packed with THOUSANDS of tiny colorful "
    "medieval fans seen from above (each a small figure with a head and "
    "shoulders, wearing tunics, cloaks, hooded caps, leather helmets and "
    "team banners in red, blue, gold, green and brown), dense organic crowd "
    "texture with subtle color variation, stone walkways and aisle gaps "
    "separating the seating sections, a few larger team-colored banners and "
    "tapestries held up by the crowd, warm even arena lighting, rich fine "
    "detail, crisp 2D top-down game-art style, no text, no logos, no "
    "watermark, no camera, no shadow of a field."
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
        # Wide arena-ish backdrop; the field (2.5:1) sits centered on top.
        queue(build(2560, 1440, STANDS_POS, "r13/stands", 20261028))
        print("  stands 2560x1440 seed=20261028", flush=True)
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
