#!/usr/bin/env python3
"""Round 13 turn 4b (user: "questi stadi sono orribili") — new strategy.

The beige sterile text-to-image stadiums were rejected. The user's reference
(faces-work/stadium/lr4njlayl49d1.jpeg) is RICH painterly fantasy art with
real texture — so we img2img FROM the reference (it inherits the rich style)
and steer the content: bare arena floor (our field goes on top), wooden
dugouts with empty benches top-left/top-right (NO numbers — those were the
turn slots), stands PACKED with a beautiful crowd.

Jobs:
  arena_i2i   seeds 501,502 denoise 0.65 | 503,504 denoise 0.75
  arena_t2i   seed  511 (original, 24 steps — checks the quality ceiling)

Usage:
    python3 r13_arena_gen.py queue
    python3 r13_arena_gen.py status
"""
import json
import sys
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"

# Round 13 turn 4c (user: "puoi anche usare dark fantasy, se vuoi") — the
# prompt now leans FULLY into the reference's dark-fantasy painterly style
# (the earlier "bright and clean" was what made the renders sterile).
ARENA = (
    "Perfectly flat top-down bird's-eye view of a DARK-FANTASY arena "
    "STADIUM (2D painterly AAA video-game map art, rich fine texture, "
    "dramatic, zero perspective, no horizon).\n"
    "CENTER: one large rectangular arena floor of BARE STONE AND PACKED "
    "EARTH — plain, uniform dark grey-brown, completely EMPTY: no grass, no "
    "grid, no lines, no markings, no numbers, no objects. It fills most of "
    "the image.\n"
    "TOP-LEFT and TOP-RIGHT: two SYMMETRIC dark-wood dugout pavilions, each "
    "a long horizontal strip of EMPTY wooden bench rows facing the arena "
    "floor. Plain aged wood only: NO numbers, NO tiles, NO text, NO "
    "symbols, NO colored squares.\n"
    "ALL OTHER STANDS and galleries (left, right and bottom bands): PACKED "
    "with thousands of beautiful, detailed spectators seen from directly "
    "above — a rich, moody, lively painterly mosaic of tiny heads in "
    "medieval tunics (deep reds, ochres, browns, forest green, muted blue, "
    "cream), dense and elegant, with a few colorful banners and pennants "
    "and a warm torchlight glow over the crowd.\n"
    "FRAME: aged dark oak planks, grey stone, iron details, deep blue cloth "
    "banners, small conical blue-slate-roofed towers in the corners, "
    "torches with warm light.\n"
    "STYLE: rich dark-fantasy painterly art, warm torchlight against cool "
    "shadows, premium AAA quality, crisp clean 2D top-down game art.\n"
    "STRICT: no text, no numbers, no letters, no logos, no watermark, no "
    "players, no grass, no pitch, no green field."
)


def i2i(denoise, seed, prefix):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "10": {"class_type": "LoadImage", "inputs": {"image": "arena_ref.jpeg"}},
        "11": {"class_type": "VAEEncode", "inputs": {"pixels": ["10", 0], "vae": ["3", 0]}},
        "4": {"class_type": "TextEncodeQwenImageEditPlus",
              "inputs": {"clip": ["2", 0], "prompt": ARENA, "image1": ["10", 0]}},
        "5": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "", "clip": ["2", 0]}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["11", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": denoise}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"images": ["8", 0], "filename_prefix": prefix}},
    }


def t2i(seed, prefix, steps=24):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"text": ARENA, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"text": "", "clip": ["2", 0]}},
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": 1536, "height": 1092, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["6", 0],
                         "seed": seed, "steps": steps, "cfg": 3.5,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 1.0}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
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
        # turn 4c: dark-fantasy prompt, denoise spread 0.75..0.85 (0.65
        # kept the reference's grass + numbered slots — too faithful; 0.75+
        # frees the floor/crowd while keeping the rich style). Three 8-step
        # jobs (~11 min each on CPU) fit the user's 1-hour PC-off window.
        for seed, dn in ((601, 0.75), (602, 0.80), (603, 0.85)):
            queue(i2i(dn, seed, "r13/arena_dk"))
            print(f"  i2i denoise={dn} seed={seed}", flush=True)
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
