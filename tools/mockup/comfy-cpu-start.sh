#!/bin/bash
# ComfyUI CPU-only start (GPU is busy with llama-server).
# Models/output/input in the shared dir; logs in the project tools dir.
cd /home/saruman/ComfyUI-Installs/ComfyUI/ComfyUI
nohup ./.venv/bin/python main.py \
  --listen 127.0.0.1 --port 8188 \
  --models-directory /home/saruman/ComfyUI-Shared/models \
  --output-directory /home/saruman/ComfyUI-Shared/output \
  --input-directory /home/saruman/ComfyUI-Shared/input \
  --cpu > /home/saruman/Projects/FUMBBL-Reborn/tools/mockup/comfy-cpu.log 2>&1 &
echo "ComfyUI starting (pid $!), log: tools/mockup/comfy-cpu.log"
