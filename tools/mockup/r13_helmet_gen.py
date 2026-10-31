#!/usr/bin/env python3
"""Round 13 turn 12 — the BLOCK MARKER helmet (user: "è orribile, rifallo
con Qwen-Image").

Side-view american football helmet, CLEAN ICON style, on a PURE WHITE
background (the white IS the marker disc — no chroma-key needed).

Usage:
    python3 r13_helmet_gen.py queue
    python3 r13_helmet_gen.py status
"""
import json
import sys
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"

HELMET = (
    "A single perfect AMERICAN FOOTBALL HELMET, full side profile view "
    "facing right, exactly centered in the middle of the image, taking "
    "about 70% of the image width.\n"
    "STYLE: premium clean game icon, smooth glossy 3D-rendered miniature "
    "look, bold simple silhouette, crisp edges, subtle soft shading, a "
    "clear dark face mask with 3 bars on the right side, a small ear hole, "
    "helmet in deep midnight blue with a white center stripe and a small "
    "gold accent line, white chin strap detail.\n"
    "BACKGROUND: pure flat solid white (#ffffff) everywhere behind the "
    "helmet, no gradient, no shadow, no ground, no vignette.\n"
    "STRICT: ONE helmet only, no text, no letters, no logo, no watermark, "
    "no second object, no border."
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
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": HELMET, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"text": "", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": 512, "height": 512, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["6", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"images": ["8", 0], "filename_prefix": "r13/helmet"}},
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


def status():
    with urllib.request.urlopen(API + "/queue", timeout=10) as r:
        d = json.load(r)
    print("queue running:", len(d.get("queue_running", [])),
          "remaining:", len(d.get("queue_remaining", [])), flush=True)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "status":
        status()
        sys.exit(0)
    for s in (901, 902, 903):
        try:
            queue(build(s))
        except urllib.error.URLError as e:
            print(f"seed {s}: {e}", flush=True)
