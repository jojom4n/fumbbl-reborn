#!/usr/bin/env python3
"""
Queue the 4 UI-direction mockups through ComfyUI (Z-Image Turbo int8, CPU),
sequentially. Writes status to mockup_status.log.
"""
import json
import os
import shutil
import time
import urllib.request

COMFY = "http://127.0.0.1:8188"
OUT_BASE = "/home/saruman/ComfyUI-Shared/output"
OUTDIR = "/tmp/mockups"
LOG = "/tmp/mockup_status.log"

STYLE = (
    " UI screenshot mockup of a Blood Bowl video game match interface, "
    "dark fantasy theme, a blood bowl field (fantasy grid board sport, NOT soccer, "
    "NOT American football) drawn as an EXACT grid of 26 columns by 15 rows "
    "of small uniform squares (an elongated rectangle clearly longer than it is wide, "
    "about 26:15 proportion, fine precise grid lines, one shaded touchdown end zone on "
    "each short end) with round player tokens "
    "(chibi cartoon miniature faces with team-colored rings and number badges), "
    "clean game UI concept art, crisp panels, wide 16:9 composition, dark moody palette"
)

JOBS = [
    ("A3_bb3_full",
     # Blood Bowl 3 direction, take 2: broadcast (NOT isometric) camera + full translucent HUD.
     "Screenshot of a 3D fantasy football video game in the style of Blood Bowl 3, during "
     "gameplay with the full user interface visible: a standard broadcast television camera "
     "from the sideline, low sports-TV angle looking diagonally across the whole pitch, a "
     "large dark fantasy stadium packed with a blurred roaring crowd, themed banners and "
     "stone architecture, floodlights, the green grass field drawn as an EXACT grid of 26 "
     "columns by 15 rows of small uniform squares with fine white grid lines (elongated "
     "rectangle, precise), full-body 3D character models of armored fantasy warriors in red "
     "and blue teams in athletic poses, a brown football visible on the field, an animated "
     "coach character standing on the sideline, OVERLAID translucent floating UI panels: a "
     "slim score bar with team names and turn number at the top center, a translucent team "
     "roster list panel on the left side, a translucent dice log panel with colored text "
     "entries and a chat panel with messages stacked on the right side, a translucent player "
     "stat card with portrait and skill icons at the bottom left, a row of square action "
     "buttons at the bottom center, dark fantasy themed borders on all panels, modern AAA 3D "
     "game graphics, wide 16:9"),
    ("B_2d_transparent",
     "A flat 2D top-down view of a BLOOD BOWL game field filling the entire screen (a fantasy "
     "grid board sport on a 26x15 board, NOT a soccer field, NOT an American football field), "
     "with the following translucent frosted-glass UI panels floating over it, each a rounded "
     "rectangle with a title bar: a team roster list panel on the left edge, a dice log panel "
     "with colored text entries on the top right, a chat panel with messages below it on the "
     "right edge, a player stats panel at the bottom left, a slim score bar with team names at "
     "the top center; glassmorphism UI with thin glowing borders and soft blur, the field "
     "clearly visible through every panel, modern esports broadcast overlay style" + STYLE),
    ("C_qds_docking",
     "A classic desktop application window layout like a developer IDE: dark dockable panels "
     "arranged around the edges, left dock with a team roster list panel, right dock with dice "
     "log and chat panels stacked vertically, bottom dock with a player info panel, each panel "
     "has its own title bar with grip and close button, tabbed dock areas, the Blood Bowl field "
     "is only partially visible in the central area, functional software UI, dark charcoal theme" + STYLE),
    ("D_fantasy_dashboard",
     "A fixed fantasy dashboard layout embedded in a dark medieval stadium: stone towers and "
     "banner flags and blurred crowd behind everything, top header bar with team portrait "
     "logos, score, turn number, timer, weather icon and fan factor bar, left panel with team "
     "roster list, center an aged parchment map style blood bowl field with double cream border "
     "and round player tokens, right column with dice log panel above a chat panel, bottom row "
     "with a player stat card (portrait, name, MA ST AG stats, skill icons) on the left and a "
     "row of square action buttons in the middle, game logo in the bottom right corner, rich "
     "moody fantasy game UI" + STYLE),
]


def log(msg):
    line = f"[{time.strftime('%H:%M:%S')}] {msg}"
    print(line, flush=True)
    with open(LOG, "a") as f:
        f.write(line + "\n")


def build_wf(prompt, prefix):
    return {
        "3": {"class_type": "UNETLoader",
              "inputs": {"unet_name": "z_image_turbo_int8_convrot.safetensors",
                         "weight_dtype": "default"}},
        "4": {"class_type": "CLIPLoader",
              "inputs": {"clip_name": "qwen_3_4b.safetensors", "type": "qwen_image"}},
        "5": {"class_type": "VAELoader", "inputs": {"vae_name": "ae.safetensors"}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["4", 0], "text": prompt}},
        "8": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["4", 0], "text": ""}},
        "9": {"class_type": "EmptySD3LatentImage",
              "inputs": {"width": 1344, "height": 768, "batch_size": 1}},
        "10": {"class_type": "KSampler",
               "inputs": {"seed": 7, "steps": 9, "cfg": 1.0, "sampler_name": "euler",
                          "scheduler": "beta", "denoise": 1.0, "model": ["3", 0],
                          "positive": ["7", 0], "negative": ["8", 0],
                          "latent_image": ["9", 0]}},
        "11": {"class_type": "VAEDecode", "inputs": {"samples": ["10", 0], "vae": ["5", 0]}},
        "12": {"class_type": "SaveImage",
               "inputs": {"filename_prefix": f"mockups/{prefix}", "images": ["11", 0]}},
    }


def queue(wf):
    req = urllib.request.Request(COMFY + "/prompt",
                                 data=json.dumps({"prompt": wf}).encode(),
                                 headers={"Content-Type": "application/json"})
    r = json.load(urllib.request.urlopen(req, timeout=60))
    if r.get("node_errors"):
        raise RuntimeError(f"node errors: {r['node_errors']}")
    return r["prompt_id"]


def wait(pid, timeout=5400):
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(10)
        try:
            d = json.load(urllib.request.urlopen(f"{COMFY}/history/{pid}", timeout=10))
        except Exception:
            continue
        if not d:
            continue
        entry = d[pid]
        if entry.get("status", {}).get("status_str") == "error":
            raise RuntimeError(f"workflow error: {entry['status'].get('messages')}")
        for node_out in entry.get("outputs", {}).values():
            imgs = node_out.get("images", [])
            if imgs:
                return imgs[0]["filename"], imgs[0]["subfolder"], imgs[0].get("type", "output")
        log(f"  still generating {int(time.time()-t0)//60}m...")
    raise TimeoutError("timeout")


def main():
    os.makedirs(OUTDIR, exist_ok=True)
    results = []
    for prefix, prompt in JOBS:
        dst = os.path.join(OUTDIR, f"{prefix}.png")
        if os.path.exists(dst):
            log(f"{prefix}: already present, skip")
            continue
        log(f"{prefix}: queueing...")
        try:
            pid = queue(build_wf(prompt, prefix))
            t0 = time.time()
            fname, sub, typ = wait(pid)
            src = OUT_BASE if typ == "output" else "/home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI/temp"
            if sub:
                src = os.path.join(src, sub)
            shutil.copyfile(os.path.join(src, fname), dst)
            log(f"{prefix}: OK in {(time.time()-t0)/60:.1f} min -> {dst}")
            results.append(prefix)
        except Exception as e:
            log(f"{prefix}: FAILED: {e}")
    log(f"=== ALL DONE: {results} ===")


if __name__ == "__main__":
    main()
