#!/usr/bin/env python3
"""Round 13 turn 5 — standalone DUGOUT PANEL asset (user's idea).

The dugout is NO LONGER baked into the stadium image: it is its own asset
(a long wooden bench panel, 16 seats, no text/numbers/symbols/players) that
the client MOUNTS onto the stadium at the manifest's dugoutHome/dugoutAway
rects. That makes the dugout UNIVERSAL: any future stadium ships its
manifest rects and the same (or another) panel mounts on it.

Style matches the approved dark_arena (dark oak + grey stone + torch glow).

Usage:
    python3 r13_dugout_panel_gen.py queue
    python3 r13_dugout_panel_gen.py status
"""
import json
import sys
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"

PANEL = (
    "Perfectly flat top-down bird's-eye view of a single long horizontal "
    "WOODEN TEAM DUGOUT BENCH panel for a dark-fantasy blood arena stadium "
    "(2D painterly AAA video-game map art, zero perspective, straight "
    "overhead). The image is a WIDE rectangle (about 2.8 times wider than "
    "tall) and the panel fills the ENTIRE image edge to edge with clean "
    "straight edges.\n"
    "CONTENT: an EMPTY team bench area seen from directly above — aged dark "
    "oak plank floor, a long horizontal wooden bench seat strip near the "
    "top edge, open floor space in front of the bench (where players stand), "
    "a low grey-stone front lip along the bottom edge, two small torch "
    "sconces with warm glowing light at the far left and right ends, a few "
    "subtle plank seams and wood grain. NO players, NO faces, NO text, NO "
    "numbers, NO symbols, NO letters, NO banners.\n"
    "STYLE: rich dark-fantasy painterly art, dark oak + grey stone + warm "
    "torch glow against cool shadow, harmonious with a medieval arena "
    "stadium, premium AAA quality, crisp clean 2D top-down game art.\n"
    "STRICT: the panel must be one clean rectangle, no perspective, no "
    "watermark, no grass."
)


def build(seed):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": PANEL, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"text": "", "clip": ["2", 0]}},
        # 1536x544 = 2.82:1 == the manifest dugout rect aspect (0.29w x 0.145h)
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": 1536, "height": 544, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["6", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"images": ["8", 0], "filename_prefix": "r13/dugout_panel"}},
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
        for seed in (701, 702):
            queue(build(seed))
            print(f"  panel seed={seed}", flush=True)
    else:
        try:
            with urllib.request.urlopen(API + "/queue", timeout=10) as r:
                q = json.load(r)
            print("pending:", len(q.get("queue_pending", [])),
                  "| running:", len(q.get("queue_running", [])))
        except urllib.error.URLError:
            print("ComfyUI not reachable at", API)


if __name__ == "__main__":
    main()
