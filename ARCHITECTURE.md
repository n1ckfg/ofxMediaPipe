# ofxMediaPipe Architecture

This document describes the architectural design and implementation details of the `ofxMediaPipe` openFrameworks addon.

## High-Level Overview

`ofxMediaPipe` serves as a bridge between **openFrameworks** and **Google's MediaPipe**. Instead of relying on MediaPipe's low-level Framework API (CalculatorGraphs), it utilizes the **MediaPipe Tasks C++ API**. This provides a simpler, higher-level interface for common machine learning vision tasks such as pose detection, face landmarking, and hand tracking.

Because MediaPipe is natively built using the Bazel build system, which is incompatible with openFrameworks' standard Make/CMake build pipeline, the addon is designed around a **pre-built shared library** dependency model.

## Directory Structure

```text
ofxMediaPipe/
├── addon_config.mk       # Addon configuration for the OF Project Generator
├── src/                  # Core openFrameworks wrapper classes
│   ├── PoseLandmarker.h  # Wrapper for mediapipe::tasks::vision::pose_landmarker
│   └── PoseLandmarker.cpp
├── libs/                 # Third-party dependencies and models
│   └── mediapipe/
│       ├── include/      # MediaPipe C++ headers
│       ├── lib/          # Pre-compiled shared libraries (e.g., linuxarmv7l/libmediapipe.so)
│       └── models/       # MediaPipe .task model files
├── examples/             # openFrameworks example projects
│   └── PoseWebcamExample/
├── README.md             # Setup and usage instructions
├── PLAN.md               # Current status and remaining tasks
└── RESEARCH.md           # Research notes and MediaPipe API details
```

## Core Components

### 1. Wrapper Classes (e.g., `PoseLandmarker`)
The addon abstracts MediaPipe's complex initialization and execution pipelines into simple, OF-friendly C++ classes.
- **Initialization (`setup`)**: Configures the underlying MediaPipe `BaseOptions` and `PoseLandmarkerOptions`, loads the `.task` model from the OF data path, and creates the task instance.
- **Inference (`detect`, `detectForVideo`)**: Takes `ofPixels` as input, converts it to a MediaPipe-compatible image format, and runs synchronous inference.

### 2. Data Conversion
MediaPipe expects image data in specific internal formats (`mediapipe::Image` backed by `mediapipe::ImageFrame`). The wrapper classes handle the translation from openFrameworks' `ofPixels`:
- Verifies the input format (e.g., 3-channel RGB).
- Wraps the raw pixel data into an `absl::make_unique<mediapipe::ImageFrame>`.
- Converts the `ImageFrame` into a `mediapipe::Image` object ready for the Tasks API.

### 3. Result Translation
MediaPipe returns deeply nested, protobuf-derived structures. `ofxMediaPipe` translates these into OF-native data structures for easier use in graphics programming:
- `NormalizedLandmark` objects are converted into standard `glm::vec3` vectors.
- Nested structures are simplified into native `std::vector` collections (e.g., `PoseLandmarkerResult`), providing intuitive access to properties like `position`, `visibility`, and `presence`.

## Build System & Dependency Strategy

Integrating Bazel-based projects into openFrameworks is historically difficult. `ofxMediaPipe` sidesteps this by decoupling the build processes:

1. **Standalone Bazel Build**: A shared library (`libmediapipe.so` or platform equivalent) is compiled separately using tools like [libmediapipe](https://github.com/cpvrlab/libmediapipe).
2. **openFrameworks Integration**: `addon_config.mk` is configured to link against this pre-built library. It specifies include paths (`libs/mediapipe/include`), library paths, and system dependencies (OpenCV, protobuf, abseil).

This architecture allows the addon to be easily integrated into user projects via the standard **openFrameworks Project Generator**.

## Execution Modes

The MediaPipe Tasks API supports different running modes. `ofxMediaPipe` conceptually wraps these to fit common OF paradigms:
- **IMAGE Mode**: (`detect(const ofPixels&)`) - For single, unrelated images. Each frame is processed independently without temporal tracking.
- **VIDEO Mode**: (`detectForVideo(const ofPixels&, int64_t timestamp_ms)`) - Ideal for webcam feeds (used in `PoseWebcamExample`). Uses temporal tracking across monotonically increasing timestamps for better performance and jitter reduction.
- **LIVE_STREAM Mode**: (Planned) - Asynchronous processing with callbacks. This will allow inference to happen on a background thread without blocking the main OF `update()` loop.
