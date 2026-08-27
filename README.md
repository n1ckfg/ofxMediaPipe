# ofxMediaPipe

An openFrameworks addon for integrating Google's MediaPipe using the MediaPipe Tasks C++ API.

## Overview

This addon provides a wrapper around MediaPipe's Tasks API (specifically for vision tasks like pose landmarking) to be used in openFrameworks projects.

**Important**: MediaPipe requires the Bazel build system to compile its libraries. This addon does not build MediaPipe itself; instead, it relies on a pre-built shared library (`libmediapipe.so`) and the associated header files.

## Setup Steps

### 1. Build the MediaPipe Shared Library

We recommend using the [libmediapipe](https://github.com/cpvrlab/libmediapipe) repository, which provides scripts to build MediaPipe as a shared library.

Follow the instructions in the libmediapipe repository to build for your platform (Linux ARM64 for Raspberry Pi).

Example for Linux ARM64 (Raspberry Pi 3/4/5):
```bash
# Clone libmediapipe
git clone https://github.com/cpvrlab/libmediapipe.git
cd libmediapipe

# Install dependencies (on Raspberry Pi OS)
sudo apt-get update
sudo apt-get install -y python3-pip python3-numpy libopencv-dev
pip3 install numpy

# Build libmediapipe for ARM64
./build-aarch64-linux.sh --version v0.10.14 --config release --opencv_dir /usr/local
```

After building, you will find the shared library and headers in a directory like:
`libmediapipe-<version>-linuxarmv7l/` (note: the script may output to a different path; check the script's output).

### 2. Copy the Built Files into the Addon

Create the directory structure in this addon:
```
ofxMediaPipe/
├── libs/
│   └── mediapipe/
│       ├── include/          # Copy the 'include' directory from the build
│       └── lib/
│           └── linuxarmv7l/  # Copy the 'lib' directory (containing libmediapipe.so) from the build
```

For example:
```bash
mkdir -p /home/pi/openFramework/of_v0.12.1_linuxaarch64_release/addons/ofxMediaPipe/libs/mediapipe/{include,lib/linuxarmv7l}
cp -r /tmp/libmediapipe/libmediapipe-*/include/* /home/pi/openFramework/of_v0.12.1_linuxaarch64_release/addons/ofxMediaPipe/libs/mediapipe/include/
cp /tmp/libmediapipe/libmediapipe-*/lib/libmediapipe.so /home/pi/openFramework/of_v0.12.1_linuxaarch64_release/addons/ofxMediaPipe/libs/mediapipe/lib/linuxarmv7l/
```

### 3. Download the Model Files

Download the desired MediaPipe Tasks model files (e.g., `pose_landmarker.task`) from the [MediaPipe Models guide](https://developers.google.com/mediapipe/solutions/vision/pose_landmarker#models) and place them in:
```
ofxMediaPipe/libs/mediapipe/models/
```

For example:
```bash
mkdir -p /home/pi/openFramework/of_v0.12.1_linuxaarch64_release/addons/ofxMediaPipe/libs/mediapipe/models
wget -O /home/pi/openFramework/of_v0.12.1_linuxaarch64_release/addons/ofxMediaPipe/libs/mediapipe/models/pose_landmarker.task \
   https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task
```

### 4. Update the addon_config.mk (if necessary)

The provided `addon_config.mk` assumes the shared library is located at `libs/mediapipe/lib/linuxarmv7l/libmediapipe.so`. If you placed it elsewhere, update the `ADDON_LIBS` or `ADDON_LDFLAGS` accordingly.

### 5. Use the Addon in Your Project

- Use the openFrameworks Project Generator to create a new project.
- Add the `ofxMediaPipe` addon to your project via the Project Generator.
- The example project `PoseWebcamExample` demonstrates how to use the `PoseLandmarker` class to detect poses from a webcam feed.

## Example: PoseWebcamExample

This example captures video from a webcam and runs pose landmark detection on each frame, drawing the detected landmarks on screen.

### Dependencies
- OpenCV (for video capture, but note that MediaPipe also requires OpenCV for its internal operations)
- The MediaPipe shared library and model files as described above.

### Usage
1. Ensure the webcam is connected and accessible.
2. Run the example. It will attempt to load the model file from `data/pose_landmarker.task` (note: the example uses `ofToDataPath` to look in the `data` folder).
3. If setup fails, check the console output for errors (e.g., missing model file, missing shared library).

## Troubleshooting

- **Shared library not found**: Ensure `libmediapipe.so` is in the correct location and that the linker flags in `addon_config.mk` or your project's `config.make` are set correctly.
- **Model file not found**: Ensure the `.task` file is in the `data` folder of your project (or adjust the path in the example).
- **Missing dependencies**: Ensure you have installed OpenCV and other dependencies as required by MediaPipe.

## Notes

- This addon currently only implements the `PoseLandmarker` class for demonstration. Similar classes can be created for face and hand landmarking.
- The example uses the `VIDEO` running mode, which requires a timestamp for each frame. The timestamp is generated using the system clock and must be monotonically increasing.
- For real-world applications, consider using a hardware timestamp or a frame counter with a known frame rate to generate timestamps.

## References

- MediaPipe Tasks C++ API: https://developers.google.com/mediapipe/solutions/vision/pose_landmarker/cpp
- libmediapipe (shared library builder): https://github.com/cpvrlab/libmediapipe
- openFrameworks: https://openframeworks.cc/