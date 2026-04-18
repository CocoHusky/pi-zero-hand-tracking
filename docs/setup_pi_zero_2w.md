# Setup Pi Zero 2 W

## OS

- Raspberry Pi OS Lite

## Base packages

```bash
sudo apt update
sudo apt install -y build-essential libsdl2-dev
```

## OpenCV

```bash
sudo apt update
sudo apt install -y libopencv-dev pkg-config
```

## TensorFlow Lite Debian package

```bash
sudo apt update
sudo apt install -y libtensorflow-lite-dev
```

Installed library path:

`/usr/lib/arm-linux-gnueabihf/libtensorflow-lite.so`

## Camera check

```bash
rpicam-hello --list-cameras
rpicam-hello -t 3000
```

## SDL local HDMI

Use:

```bash
SDL_VIDEODRIVER=kmsdrm ./app_binary
```


## Run V5 UHD person detector

Use an absolute ONNX model path from the UHD repository export:

```bash
./scripts/run_v5_person_uhd.sh /absolute/path/to/uhd_person.onnx
```
