#!/usr/bin/env python3
"""Round 13 — Qwen-Image 2.1 Turbo IMG2IMG for the block dice.

Shows the model the REAL die face photo (reference in ComfyUI input/) and
steers it to a flat, top-down, crimson-red vector die face with the SAME
white spiky badge + symbol design. This should be more faithful to the
official GW dice than pure text2image (the exact spike pattern and symbol
come from the reference).

Usage:
    python3 r13_img2img.py <ref_png> <out_prefix> [denoise] [seed] [symbol]
    python3 r13_img2img.py --test      # one ref at a few denoise levels

The reference PNG must already be copied into
/home/saruman/ComfyUI-Shared/input/
"""
import json
import sys
import urllib.request
import urllib.error

API = "http://127.0.0.1:8188"

DIE_RED = ("a solid flat crimson red (around #B02020), completely uniform, "
           "no gradient, no bevel, no 3D perspective, no texture noise, no shadow")

def base_style(die, symbol):
    return (
        "A single square game die face, perfectly flat top-down view, the die "
        "face filling the ENTIRE frame edge to edge as a perfect square with "
        "slightly rounded corners. The die background is {die}. Flat clean 2D "
        "vector game-art style, crisp uniform edges, high contrast. "
        "In the exact center of the die face: a WHITE badge, a circular disc "
        "about 72% of the face width, whose edge is a SPIKY BURST: 11 to 12 "
        "SHORT, POINTED, ANGULAR triangular spikes of VARYING length (some "
        "clearly longer, some shorter, uneven) radiating outward, each spike "
        "a sharp triangle with a slightly soft tip but clearly ANGULAR and "
        "POINTED, like a comic-book explosion star or a torn-paper burst; the "
        "edge must be SPIKY and angular, NOT a smooth scalloped/flower edge, "
        "NOT a wavy border, NOT rounded bumps. The interior of the badge is "
        "solid clean white. "
        "Inside the badge, perfectly centered, LARGE (about 75% of the badge), "
        "in solid crimson red (the die color): {sym}. "
        "Nothing else: no text, no numbers, no letters, no watermark, no "
        "border frame, no second badge, no background objects, no other dice."
    ).format(die=die, sym=symbol)


def build_img2img(ref_name, pos, denoise, seed, prefix):
    return {
        "1": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "qwen_image_2.1_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen3vl_8b_int8_convrot.safetensors",
                         "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "10": {"class_type": "LoadImage",
               "inputs": {"image": ref_name}},
        "11": {"class_type": "VAEEncode",
                "inputs": {"pixels": ["10", 0], "vae": ["3", 0]}},
        "4": {"class_type": "CLIPTextEncode",
              "inputs": {"text": pos, "clip": ["2", 0]}},
        "5": {"class_type": "CLIPTextEncode",
              "inputs": {"text": "", "clip": ["2", 0]}},
        "7": {"class_type": "KSampler",
              "inputs": {"model": ["1", 0], "positive": ["4", 0],
                         "negative": ["5", 0], "latent_image": ["11", 0],
                         "seed": seed, "steps": 8, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple",
                         "denoise": denoise}},
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


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "--test":
        # one reference, a few denoise levels to find the sweet spot
        ref = "ref1_skull.png"
        sym = ("a bold filled skull in a clean fantasy game-logo style: a "
               "rounded domed cranium, two large slanted angular angry eye "
               "sockets, a small inverted-triangle nasal cavity, and a lower "
               "jaw with four square teeth; the eye sockets, nose and tooth "
               "gaps are white cutouts, the skull solid flat red")
        for dn in (0.70, 0.85, 0.95):
            pos = base_style(DIE_RED, sym)
            queue(build_img2img(ref, pos, dn, 42, f"r13/i2i_test_{int(dn*100)}"))
            print(f"  {ref} denoise={dn}", flush=True)
        sys.exit(0)

    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    ref, prefix, denoise = sys.argv[1], sys.argv[2], float(sys.argv[3])
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 7
    sym = sys.argv[5] if len(sys.argv) > 5 else ""
    pos = base_style(DIE_RED, sym)
    queue(build_img2img(ref, pos, denoise, seed, prefix))
