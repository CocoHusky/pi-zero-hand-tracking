#!/usr/bin/env bash
set -euo pipefail

MODEL_PATH="${1:-/home/user/models/uhd_person.onnx}"

cd "$(dirname "$0")/../apps/v5_person_uhd"
make
SDL_VIDEODRIVER=kmsdrm ./person_uhd --model "$MODEL_PATH" --width 320 --height 240 --fps 15 --input 320 --conf 0.35 --nms 0.45 --detect-every 2 --mirror
