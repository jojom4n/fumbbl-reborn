#!/usr/bin/env python3
"""Round 12 — Qwen-Image 2.1 Turbo generations (proven recipe, see
tools/mockup/f_field_queue.py from 21/ott that produced the approved
field.png):
  1. field.png   (1344x768)  — the 26x15 Blood Bowl pitch: realistic grass,
                               mowing stripes, crimson TD bands + TOUCHDOWN,
                               faint grid. NO baked center line (the code
                               draws it exactly), NO rain sheen (the rain is
                               now an ANIMATED code overlay).
  2. dugout.png  (1024x2048) — team dugout bench zone (earthy/soil theme,
                               3 equal bands RESERVE / KO / INJURED, empty).
Both are SEPARATE assets: the dugout is identical for every future field
type (user requirement: multiple field themes, one shared dugout).
"""
import json, os, shutil, sys, time, urllib.request

COMFY = "http://127.0.0.1:8188"
OUT_DIR = "/home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI/output/r12"
DEST = "/tmp/r12_gen"
os.makedirs(DEST, exist_ok=True)

FIELD_POS = (
    "Spectacular AAA fantasy sports video game field, perfectly flat top-down "
    "view filling the ENTIRE frame edge to edge with no border and no margin. "
    "It is a blood bowl pitch (a fantasy grid board sport, NOT soccer, NOT "
    "american football): rich natural green grass with visible individual "
    "grass blades, a realistic mowing pattern of alternating vertical light "
    "and dark green stripes running along the length of the field, subtle "
    "worn patches and light scuff marks in the middle of the field, dry "
    "grass in soft daylight. A faint precise white grid of exactly 26 "
    "columns and 15 rows of squares covers the whole field (an elongated "
    "rectangle, clearly longer than wide). At each of the two short ends, "
    "the outermost column of the grid is a touchdown zone painted as a deep "
    "crimson-red banner band spanning the full height of the field, carrying "
    "ONLY the word TOUCHDOWN in huge bold white display letters running "
    "vertically down the band, with no skulls and no other icons. Dramatic "
    "floodlight ambiance, hyper-detailed grass texture, cinematic AAA game "
    "quality. STRICT: no players, no ball, no team logos, no emblems, no "
    "watermarks, no numbers, no grid coordinates, no stadium, no crowd, no "
    "user interface, no text other than the two TOUCHDOWN words, no center "
    "line, no rain, no puddles, no wet grass."
)

DUGOUT_POS = (
    "Top-down flat orthographic view of a team dugout bench area for a "
    "fantasy football board game, filling the entire frame edge to edge. "
    "A tall narrow vertical panel. The whole panel is covered by a rich "
    "earthy soil and dirt texture: packed warm brown earth with subtle raked "
    "soil ridges, scattered small pebbles and stones, tiny bits of dried "
    "grass and straw, a few darker damp patches. A thick weathered dark "
    "wooden frame border surrounds the panel on all four sides, with visible "
    "wood grain and nail heads. Two horizontal weathered wooden beams cross "
    "the panel, dividing it into exactly three equal horizontal bands, each "
    "band one third of the panel height. In the TOP band the label RESERVE "
    "is written in bold white stenciled capital letters, positioned in the "
    "UPPER part of the band (near its top edge), horizontally centered, "
    "leaving the whole area below the label empty. In the MIDDLE band the "
    "label KO is written in bold white stenciled capital letters in the "
    "UPPER part of the band, horizontally centered, leaving the area below "
    "empty. In the BOTTOM band the label INJURED is written in bold white "
    "stenciled capital letters in the UPPER part of the band, horizontally "
    "centered, leaving the area below empty. The bands are otherwise "
    "completely empty: no figures, no minis, no tokens, no chairs, no "
    "people. Warm earthy color palette of browns, tans and dark wood. Flat "
    "top-down view, no perspective, no horizon, no sky. Game board texture "
    "asset, high detail, hyper-detailed wood and soil textures."
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
              "inputs": {"images": ["8", 0],
                         "filename_prefix": prefix}},
    }


def queue(wf):
    req = urllib.request.Request(COMFY + "/prompt", data=json.dumps({"prompt": wf}).encode(),
                                 headers={"Content-Type": "application/json"})
    r = json.load(urllib.request.urlopen(req, timeout=30))
    if r.get("node_errors"):
        raise RuntimeError(f"node errors: {r['node_errors']}")
    return r["prompt_id"]


def wait_history(pid, timeout=3600):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            h = json.load(urllib.request.urlopen(COMFY + "/history/" + pid, timeout=30))
        except Exception:
            time.sleep(5)
            continue
        if pid in h:
            e = h[pid]
            if e.get("status", {}).get("completed"):
                return e
            if e.get("status", {}).get("status_str") == "error":
                raise RuntimeError(f"job error: {json.dumps(e.get('status'))[:500]}")
        time.sleep(5)
    raise TimeoutError(pid)


def main():
    jobs = sys.argv[1:] or ["field", "dugout"]
    for name in jobs:
        if name == "field":
            wf = build_workflow(1344, 768, FIELD_POS, "r12/field_v2", 20261026)
        else:
            wf = build_workflow(1024, 2048, DUGOUT_POS, "r12/dugout_v2", 30303030)
        pid = queue(wf)
        print(f"[{name}] queued {pid}", flush=True)
        wait_history(pid)
        # copy from the output dir (subfolder r12)
        for fn in os.listdir(OUT_DIR):
            if fn.startswith("dugout_v3"):
                shutil.copy(os.path.join(OUT_DIR, fn),
                            os.path.join(DEST, f"{name}_{fn}"))
                print(f"[{name}] saved {fn}", flush=True)


if __name__ == "__main__":
    main()
