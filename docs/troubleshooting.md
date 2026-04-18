# Troubleshooting

## No camera image

- Check `rpicam-hello --list-cameras`
- Check `/tmp/...log`
- Verify CSI camera seating

## Black screen

- Use SDL + KMSDRM locally on HDMI
- Avoid relying on X11 / Wayland

## TFLite source build failure

If the compiler is killed during build, that strongly suggests memory pressure / OOM on Pi Zero 2 W.

## TFLite C++ API compile errors

Prefer the TFLite C API if revisiting TFLite integration.

## ONNX model load failure

- Use absolute paths
- Do not use `~` in model paths passed to the app
- Try full model before quantized model

## HOG detects no person

- Stand farther back
- Ensure most of the body is visible
- Wide/fisheye framing hurts HOG performance


## V5 UHD model shows no detections

- Confirm `--model` uses an absolute path to an ONNX file
- Try lowering `--conf` to `0.25`
- If class mapping differs, set `--person-class-id` to the model's person class index
- Increase input size (`--input 416` or `--input 640`) if CPU budget allows
