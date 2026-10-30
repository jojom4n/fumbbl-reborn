#!/usr/bin/env python3
"""GW 2025 dugout symbols — Qwen-Image i2i reproduction (CPU).

The official Games Workshop Blood Bowl 3rd Season (2025) dugouts carry three
red symbols inside a spiked red ring (official product photos, saved under
faces-work/stadium/gw2025_refs/):
  * RESERVES  = chunky up arrow
  * KO        = dazed/stunned face with stars circling the head (NOT angry)
  * INJURED   = skull + crossed bones

Per the permanent AGENTS.md rule, every graphic asset is made with
Qwen-Image (never hand-drawn in code). We feed the cleaned official crops
(sym_refs) into Qwen-Image Edit (image-to-image) so the result is
near-identical to the official artwork but a clean flat vector-style icon
on white (the client keeps it in one red; team tinting is done at render
time if ever needed).

Usage:
    python3 sym_queue.py queue     # queue 3 symbols x 2 seeds
    python3 sym_queue.py status
"""
import json
import sys
import urllib.request

API = "http://127.0.0.1:8188"

RING = ("a crimson red circular ring with sharp triangular spikes pointing "
        "outward all around its rim, the ring in solid crimson red with a "
        "thin white outline")

PROMPTS = {
    "reserve_arrow": (
        "Redraw this exact red game symbol as a clean flat 2D vector-style "
        "game icon, perfectly centered on a pure white background, filling "
        "about 85% of the frame: a chunky bold crimson red UPWARD ARROW with "
        "a thin white outline around the arrow, inside {ring}. The arrow "
        "shaft is a clean rectangle with a flat bottom and a wide triangular "
        "head. Flat solid crimson red, crisp uniform edges, no gradients, no "
        "3D, no texture noise, no shadow. No text, no numbers, no letters, "
        "no watermark, nothing else in the frame."
    ).format(ring=RING),
    "ko_face": (
        "Redraw this exact red game symbol as a clean flat 2D vector-style "
        "game icon, perfectly centered on a pure white background, filling "
        "about 85% of the frame: a DAZED STUNNED FACE in crimson red — "
        "brow furrowed in dizziness, eyes squinting, mouth wide open in a "
        "dazed groan with gritted teeth, and small five-pointed stars "
        "circling above the head (classic dizzy/stunned look, NOT an angry "
        "face) — inside {ring}. Flat solid crimson red with thin white "
        "outlines, crisp uniform edges, no gradients, no 3D, no texture "
        "noise, no shadow. No text, no numbers, no letters, no watermark, "
        "nothing else in the frame."
    ).format(ring=RING),
    "injured_skull": (
        "Redraw this exact red game symbol as a clean flat 2D vector-style "
        "game icon, perfectly centered on a pure white background, filling "
        "about 85% of the frame: a bold crimson red SKULL with two CROSSED "
        "BONES behind it (rounded cranium, two round eye sockets, nasal "
        "cavity, grinning lower jaw with teeth; the two bones cross in an X "
        "behind the skull, bone ends with the classic double-knob tips) "
        "inside {ring}. Flat solid crimson red with thin white outlines, "
        "crisp uniform edges, no gradients, no 3D, no texture noise, no "
        "shadow. No text, no numbers, no letters, no watermark, nothing "
        "else in the frame."
    ).format(ring=RING),
}

SEEDS = {
    "reserve_arrow": [301, 302],
    "ko_face": [303, 304],
    "injured_skull": [305, 306],
}


def workflow(name, seed):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "4": {"class_type": "LoadImage",
              "inputs": {"image": f"{name}.png"}},
        "5": {"class_type": "VAEEncode",
              "inputs": {"pixels": ["4", 0], "vae": ["3", 0]}},
        "6": {"class_type": "TextEncodeQwenImageEditPlus",
              "inputs": {"clip": ["2", 0], "prompt": PROMPTS[name],
                         "image1": ["4", 0]}},
        "7": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "", "clip": ["2", 0]}},
        "8": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["6", 0],
                         "negative": ["7", 0], "latent_image": ["5", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": 0.8}},
        "9": {"class_type": "VAEDecode",
              "inputs": {"samples": ["8", 0], "vae": ["3", 0]}},
        "10": {"class_type": "SaveImage",
               "inputs": {"images": ["9", 0],
                          "filename_prefix": f"mockups/symbols_v1/{name}"}},
    }


def queue_all():
    for name, seeds in SEEDS.items():
        for seed in seeds:
            body = json.dumps({"prompt": workflow(name, seed)}).encode()
            req = urllib.request.Request(
                API + "/prompt", data=body,
                headers={"Content-Type": "application/json"})
            with urllib.request.urlopen(req, timeout=30) as r:
                d = json.load(r)
            print(f"queued {name} seed={seed} -> {d.get('prompt_id')}")


def status():
    with urllib.request.urlopen(API + "/queue", timeout=15) as r:
        d = json.load(r)
    print("pending", len(d.get("queue_pending", [])),
          "running", len(d.get("queue_running", [])))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
    if cmd == "queue":
        queue_all()
    else:
        status()
