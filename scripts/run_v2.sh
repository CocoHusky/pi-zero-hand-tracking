#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../apps/v2_swipe_simple"
make
SDL_VIDEODRIVER=kmsdrm ./swipe_v2 --width 320 --height 240 --fps 60
