#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../apps/person_tracker_hog"
make
SDL_VIDEODRIVER=kmsdrm ./person_tracker_hog --width 320 --height 240 --fps 15
