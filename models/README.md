# Models

## Hand models tested

- palm_detection_full.tflite
- hand_landmark.tflite
- hand_landmark_full.tflite

These were too slow for practical use on Pi Zero 2 W.

## Person models tested

- person_detection_mediapipe_2023mar.onnx
- person_detection_mediapipe_2023mar_int8bq.onnx

Notes:

- use absolute paths
- quantized ONNX path failed in current OpenCV 4.10 environment


## UHD person model

Source repo: https://github.com/PINTO0309/UHD/

Use an exported ONNX model with an absolute runtime path, for example:

`/home/user/models/uhd_person.onnx`
