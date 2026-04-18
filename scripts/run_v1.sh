#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../apps/v1_camera_viewer"
make
SDL_VIDEODRIVER=kmsdrm ./camera_viewer --width 320 --height 240 --fps 60
