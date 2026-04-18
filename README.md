# Pi Zero Vision

Low-latency local-HDMI vision apps for Raspberry Pi Zero 2 W with OV5647 camera.

## Goals

- Fullscreen local HDMI camera display
- Lowest practical latency
- Classical tracking baseline that actually works on Pi Zero 2 W
- Experimental ML backends for person and hand detection

## Hardware

- Raspberry Pi Zero 2 W
- OV5647 CSI camera
- Local HDMI display

## Proven conclusions

- SDL2 + KMSDRM + `rpicam-vid` is the correct display pipeline
- Classical tracking is the practical production path
- TFLite hand landmark inference is too slow on this board
- ONNX quantized person model had compatibility issues in this environment
- HOG is lightweight but unreliable for this camera/framing setup

## Apps

- `apps/v1_camera_viewer`: working fullscreen camera viewer
- `apps/v2_swipe_simple`: basic motion swipe tracker
- `apps/v3_swipe_blob`: improved low-latency swipe tracker
- `apps/v4_tflite_hand_experimental`: experimental hand landmark ML
- `apps/person_tracker_hog`: HOG person detector
- `apps/person_tracker_onnx`: experimental ONNX person detector
- `apps/v5_person_uhd`: UHD model-based person detector with HDMI overlay confidence

## Quick start

```bash
./scripts/install_base.sh
./scripts/run_v1.sh
```

## Recommended production path

- Gesture tracking on Pi Zero 2 W: `apps/v3_swipe_blob`
- Person detection on Pi Zero 2 W: `apps/v5_person_uhd` (UHD ONNX model, confidence overlay)
