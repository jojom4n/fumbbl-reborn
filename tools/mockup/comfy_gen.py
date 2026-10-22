#!/usr/bin/env python3
"""
Queue a Qwen-Image 2.1 (GGUF, CPU) workflow through the local ComfyUI API and
wait for the output. Mirrors faces-work/gen_faces_batch.py.

Usage: comfy_gen.py <prompt_file_or_-> <width> <height> <outdir> [prefix]
  prompt read from file, or from stdin when prompt_file is "-"
"""
import json
import os
import shutil
import sys
import time
import urllib.request

COMFY = "http://127.0.0.1:8188"
OUT_BASE = "/home/saruman/ComfyUI-Shared/output"


def queue(wf):
    req = urllib.request.Request(
        COMFY + "/prompt",
        data=json.dumps({"prompt": wf}).encode(),
        headers={"Content-Type": "application/json"},
    )
    r = json.load(urllib.request.urlopen(req, timeout=60))
    if r.get("node_errors"):
        raise RuntimeError(f"node errors: {r['node_errors']}")
    return r["prompt_id"]


def wait(pid, timeout=7200):
    t0 = time.time()
    last = ""
    while time.time() - t0 < timeout:
        time.sleep(5)
        try:
            d = json.load(urllib.request.urlopen(f"{COMFY}/history/{pid}", timeout=10))
        except Exception:
            continue
        if not d:
            continue
        entry = d[pid]
        status = entry.get("status", {})
        if status.get("status_str") == "error":
            raise RuntimeError(f"workflow error: {status.get('messages')}")
        # live progress log
        for m in status.get("messages", []):
            if m[0] == "execution_starting":
                last = f"queued {int(time.time()-t0)}s"
        if entry.get("outputs"):
            for node_out in entry["outputs"].values():
                imgs = node_out.get("images", [])
                if imgs:
                    return imgs[0]["filename"], imgs[0]["subfolder"], imgs[0].get("type", "output")
        print(f"  ... {int(time.time()-t0)}s elapsed ({last})", flush=True)
    raise TimeoutError(f"no output after {timeout}s")


def build_wf(prompt, width, height, seed, prefix, steps=8):
    # Qwen-Image 2.1 Turbo (official recipe: 8 steps, cfg 1, euler/fixed)
    # via ComfyUI-GGUF loaders (UNet + Qwen3-VL 8B text encoder as GGUF).
    return {
        "1": {"class_type": "UnetLoaderGGUF",
              "inputs": {"unet_name": "qwen-image-2.1-turbo-Q8_0.gguf"}},
        "2": {"class_type": "CLIPLoaderGGUF",
              "inputs": {"clip_name": "Qwen3-VL-8B-Instruct-Q4_K_M.gguf",
                         "type": "qwen_image"}},
        "3": {"class_type": "VAELoader",
              "inputs": {"vae_name": "qwen_image_2.1_vae_bf16.safetensors"}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["2", 0], "text": prompt}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["2", 0], "text": ""}},
        "6": {"class_type": "EmptyLatentImage",
              "inputs": {"width": width, "height": height, "batch_size": 1}},
        "7": {"class_type": "KSampler",
              "inputs": {"seed": seed, "steps": steps, "cfg": 1.0,
                         "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0,
                         "model": ["1", 0], "positive": ["4", 0], "negative": ["5", 0],
                         "latent_image": ["6", 0]}},
        "8": {"class_type": "VAEDecode", "inputs": {"samples": ["7", 0], "vae": ["3", 0]}},
        "9": {"class_type": "SaveImage",
              "inputs": {"filename_prefix": prefix, "images": ["8", 0]}},
    }


def main():
    prompt_arg, width, height, outdir = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    prefix = sys.argv[5] if len(sys.argv) > 5 else "mockup"
    steps = int(sys.argv[6]) if len(sys.argv) > 6 else 8
    seed = int(sys.argv[7]) if len(sys.argv) > 7 else 42
    prompt = sys.stdin.read() if prompt_arg == "-" else open(prompt_arg).read().strip()

    os.makedirs(outdir, exist_ok=True)
    wf = build_wf(prompt, width, height, seed, prefix, steps)
    pid = queue(wf)
    print(f"queued {pid}", flush=True)
    t0 = time.time()
    fname, sub, typ = wait(pid)
    dt = time.time() - t0
    src = OUT_BASE if typ == "output" else "/home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI/temp"
    if sub:
        src = os.path.join(src, sub)
    dst = os.path.join(outdir, f"{prefix}.png")
    shutil.copyfile(os.path.join(src, fname), dst)
    print(f"OK {dst} in {dt/60:.1f} min ({width}x{height}, {steps} steps)", flush=True)


if __name__ == "__main__":
    main()
