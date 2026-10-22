#!/usr/bin/env python3
"""Mockup F — spectacular FIELD (Qwen-Image 2.1 Turbo).

The field is generated ALONE (no players, no UI, no logos, no coordinates) so
it is a clean, maximum-quality texture layer. The mockup compositor
(compose_f.py) then adds: real FUMBBL team logos (per-match, ~12% opacity,
center of each half), real player tokens at the live-match coordinates, the
crisp light-style UI chrome (header / boxes / ticker / chat button / card /
radial wheel) and the TOUCHDOWN banner text.

Prompt goals (per final UI spec, 22/ott — "spettacolare"):
  * real grass texture (visible blades), mowing stripes
  * rain sheen (the live match weather is Pouring Rain)
  * gorgeous TOUCHDOWN zones at both ends (banner + skulls + big letters)
  * thick crisp center line, fine precise 26x15 grid
  * NO coordinates, NO team logos, NO players, NO ball, NO UI

Usage: python3 f_field_queue.py [all|<seed>]
"""
import json
import sys
import urllib.request

API = "http://127.0.0.1:8188"

PROMPT = (
    "Spectacular AAA fantasy sports video game field, perfectly flat top-down "
    "view filling the ENTIRE frame edge to edge with no border and no margin. "
    "It is a blood bowl pitch (a fantasy grid board sport, NOT soccer, NOT "
    "american football): rich natural green grass with visible individual "
    "grass blades, a realistic mowing pattern of alternating vertical light "
    "and dark green stripes running along the length of the field, subtle "
    "worn patches and light scuff marks in the middle of the field, wet "
    "rainy grass with a soft sheen and tiny light reflections (pouring "
    "rain). A fine precise white grid of exactly 26 columns and 15 rows of "
    "squares covers the whole field (an elongated rectangle, clearly longer "
    "than wide). A thick crisp solid white center line divides the field "
    "exactly in half, from the top edge to the bottom edge. At each of the "
    "two short ends, the outermost column of the grid is a touchdown zone "
    "painted as a deep crimson-red banner band spanning the full height of "
    "the field, decorated with a white skull icon and the word TOUCHDOWN in "
    "huge bold white display letters running vertically down the band. "
    "Dramatic floodlight ambiance, hyper-detailed grass texture, cinematic "
    "AAA game quality, wide 16:9. STRICT: no players, no ball, no team "
    "logos, no emblems, no watermarks, no numbers, no grid coordinates, no "
    "stadium, no crowd, no user interface, no text other than the two "
    "TOUCHDOWN words."
)

SEEDS = [71, 72]


def build_workflow(seed):
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
              "inputs": {"text": PROMPT, "clip": ["2", 0]}},
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
                         "filename_prefix": f"mockups/F_field_s{seed}"}},
    }


def queue(seed):
    body = json.dumps({"prompt": build_workflow(seed)}).encode()
    req = urllib.request.Request(API + "/prompt", data=body,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        d = json.load(r)
    print("queued", d.get("prompt_id"), "seed", seed)
    return d.get("prompt_id")


def main():
    sel = sys.argv[1] if len(sys.argv) > 1 else "all"
    seeds = SEEDS if sel == "all" else [int(sel)]
    for s in seeds:
        queue(s)
    print(f"{len(seeds)} field job(s) queued")


if __name__ == "__main__":
    main()
