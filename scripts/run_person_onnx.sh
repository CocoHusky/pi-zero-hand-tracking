#!/usr/bin/env bash
set -euo pipefail

MODEL_PATH="${1:-/home/user/ml-person/models/person_detection_mediapipe_2023mar.onnx}"

cd "$(dirname "$0")/../apps/person_tracker_onnx"
make
SDL_VIDEODRIVER=kmsdrm ./person_tracker_onnx "$MODEL_PATH"
