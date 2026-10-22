#!/usr/bin/env python3
"""Generate ONE mockup by name (A_3d | B_2d_transparent | C_qds_docking | D_fantasy_dashboard).
Usage: gen_one.py <prefix>
"""
import sys
import mockup_queue as mq

def main():
    prefix = sys.argv[1]
    job = next((j for j in mq.JOBS if j[0] == prefix), None)
    if not job:
        sys.exit(f"unknown prefix {prefix}; options: {[j[0] for j in mq.JOBS]}")
    prefix, prompt = job
    mq.log(f"{prefix}: queueing (Z-Image Turbo, 1344x768, CPU)...")
    pid = mq.queue(mq.build_wf(prompt, prefix))
    import time
    t0 = time.time()
    fname, sub, typ = mq.wait(pid)
    import os, shutil
    os.makedirs(mq.OUTDIR, exist_ok=True)
    src = mq.OUT_BASE if typ == "output" else "/home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI/temp"
    if sub:
        src = os.path.join(src, sub)
    dst = os.path.join(mq.OUTDIR, f"{prefix}.png")
    shutil.copyfile(os.path.join(src, fname), dst)
    mq.log(f"{prefix}: OK in {(time.time()-t0)/60:.1f} min -> {dst}")

if __name__ == "__main__":
    main()
