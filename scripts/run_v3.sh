#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../apps/v3_swipe_blob"
make
SDL_VIDEODRIVER=kmsdrm ./swipe_v3 --width 320 --height 240 --fps 60
