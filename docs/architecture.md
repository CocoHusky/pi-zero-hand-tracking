# Architecture

## Camera ingest

All apps use:

- `rpicam-vid`
- `--codec yuv420`
- stdout pipe into the app

This avoids browser, Flask, and high-overhead GUI stacks.

## Display

All local display apps use:

- SDL2
- `SDL_VIDEODRIVER=kmsdrm`
- fullscreen desktop window
- YUV texture upload

## Tracking

### Classical path

- low internal resolution
- ROI
- motion / blob extraction
- centroid smoothing
- swipe direction classification

### ML path

- optional only
- model backend must fail gracefully
- never block the entire app if model load or inference fails

### Person detection path (V5 UHD)

- OpenCV DNN ONNX runtime on CPU
- Person-class filtering + NMS
- SDL2 overlay with confidence digits rendered on HDMI output

## Why not browser / Flask / Qt for hot loop

Those paths were higher overhead and not suitable for the target latency on Pi Zero 2 W.

## Why classical tracking is the baseline

Hand landmark ML was too slow and unreliable on-device.
