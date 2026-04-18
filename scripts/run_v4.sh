#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../apps/v4_tflite_hand_experimental"
make
SDL_VIDEODRIVER=kmsdrm ./v4_tflite
