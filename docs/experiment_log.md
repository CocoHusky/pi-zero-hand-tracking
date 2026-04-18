# Experiment Log

## Successes

- OV5647 camera detected and streamed
- SDL/KMSDRM fullscreen local display works
- C++ camera viewer works
- Classical swipe tracking works
- HOG person detector runs at usable speed
- V5 UHD ONNX path added for person detection with confidence overlay output on HDMI

## Failures / limitations

- Python/Flask/browser path too high overhead
- Python local display path too slow
- TensorFlow Lite source build failed due to likely OOM during FlatBuffers compilation.
- TFLite C++ wrapper integration failed due to header/type conflicts
- TFLite hand landmark runtime on Pi Zero 2 W was around 2 seconds per inference and often returned no hand.
- OpenCV ONNX int8 model path failed in the current environment.
- OpenCV Zoo demo also failed when given `~` in the model path because the file path was not expanded by the app.
- HOG did not reliably detect the person

## Current recommendation

- Production: `v3_swipe_blob`
- Experimental ML: person-only ONNX backend
