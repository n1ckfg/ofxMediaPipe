# ofxMediaPipe Addon Status Report

## Current State
- Addon directory structure exists at `addons/ofxMediaPipe/`
- Source files present: `ofxMediaPipe.cpp`, `FaceLandmarker.cpp`, `HandLandmarker.cpp`, `PoseLandmarker.cpp`
- `addon_config.mk` configured with includes, linker flags, and source list
- Example project: `examples/PoseWebcamExample/`

## Missing Components
1. **Shared library** (`libmediapipe.so`) — missing from `libs/mediapipe/lib/linuxarmv7l/`
2. **Model files** (`.task`) — missing from `libs/mediapipe/models/`

## Required Actions
1. Build `libmediapipe.so` for Linux ARM64 using [cpvrlab/libmediapipe](https://github.com/cpvrlab/libmediapipe):
   ```bash
   git clone https://github.com/cpvrlab/libmediapipe.git
   cd libmediapipe
   sudo apt-get update && sudo apt-get install -y python3-pip python3-numpy libopencv-dev
   pip3 install numpy
   ./build-aarch64-linux.sh --version v0.10.14 --config release --opencv_dir /usr/local
   ```
2. Copy built artifacts:
   ```bash
   mkdir -p libs/mediapipe/{include,lib/linuxarmv7l,models}
   cp -r /path/to/libmediapipe-build/include/* libs/mediapipe/include/
   cp /path/to/libmediapipe-build/lib/libmediapipe.so libs/mediapipe/lib/linuxarmv7l/
   ```
3. Download model files:
   ```bash
   wget -O libs/mediapipe/models/pose_landmarker.task \
     https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task
   ```

## Verification
After completing the above, build and run `examples/PoseWebcamExample` to confirm the addon works.