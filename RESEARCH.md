# MediaPipe C++ Specification for openFrameworks Addon

## Overview

MediaPipe is a C++ pipeline framework for building multimodal ML pipelines. The core is implemented in C++ with Bazel as the build system. For openFrameworks integration, there are two main approaches:

1. **MediaPipe Tasks API (Recommended)** - High-level C/C++ API for common vision tasks (Face Mesh, Hands, Pose, etc.)
2. **MediaPipe Framework API (Low-level)** - Full graph/calculator control for custom pipelines

---

## 1. MediaPipe Tasks API (C/C++)

### 1.1 Architecture
- **MediaPipe Tasks**: Cross-platform APIs for deploying solutions
- **Pre-trained Models**: Ready-to-run `.task` model bundles with metadata
- **Three Running Modes**:
  - `IMAGE` - Single image inference
  - `VIDEO` - Video frame sequence (monotonic timestamps)
  - `LIVE_STREAM` - Async callback for real-time camera input

### 1.2 Available Vision Tasks (C++ Headers)
| Task | C++ Header | C Header |
|------|------------|----------|
| Face Landmarker | `mediapipe/tasks/cc/vision/face_landmarker/face_landmarker.h` | `mediapipe/tasks/c/vision/face_landmarker/face_landmarker.h` |
| Hand Landmarker | `mediapipe/tasks/cc/vision/hand_landmarker/hand_landmarker.h` | `mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker.h` |
| Pose Landmarker | `mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h` | `mediapipe/tasks/c/vision/pose_landmarker/pose_landmarker.h` |

### 1.3 Key C++ Classes (FaceLandmarker Example)
```cpp
// Options configuration
struct FaceLandmarkerOptions {
    tasks::core::BaseOptions base_options;  // Model path, accelerator, op resolver
    core::RunningMode running_mode = core::RunningMode::IMAGE;
    int num_faces = 1;
    float min_face_detection_confidence = 0.5f;
    float min_face_presence_confidence = 0.5f;
    float min_tracking_confidence = 0.5f;
    bool output_face_blendshapes = false;
    bool output_facial_transformation_matrixes = false;
    std::function<void(absl::StatusOr<FaceLandmarkerResult>, const Image&, int64_t)> result_callback;
};

// Main class
class FaceLandmarker : public tasks::vision::core::BaseVisionTaskApi {
public:
    static absl::StatusOr<std::unique_ptr<FaceLandmarker>> Create(
        std::unique_ptr<FaceLandmarkerOptions> options);
    
    // IMAGE mode
    absl::StatusOr<FaceLandmarkerResult> Detect(
        Image image,
        std::optional<core::ImageProcessingOptions> options = std::nullopt);
    
    // VIDEO mode
    absl::StatusOr<FaceLandmarkerResult> DetectForVideo(
        Image image, int64_t timestamp_ms,
        std::optional<core::ImageProcessingOptions> options = std::nullopt);
    
    // LIVE_STREAM mode
    absl::Status DetectAsync(
        Image image, int64_t timestamp_ms,
        std::optional<core::ImageProcessingOptions> options = std::nullopt);
    
    absl::Status Close();
};
```

### 1.4 Data Types
- `mediapipe::Image` - RGB/RGBA image wrapper (CPU/GPU)
- `NormalizedRect` - Region of interest (rotation, not ROI)
- `FaceLandmarkerResult` - Contains landmarks, blendshapes, transformation matrices
- `Timestamp` - Microseconds since epoch (monotonic per stream)

### 1.5 Model Files
Download from: https://developers.google.com/mediapipe/solutions/vision/face_landmarker#models
- `face_landmarker.task` (or `hand_landmarker.task`, `pose_landmarker.task`)
- Place in `bin/data/` folder

---

## 2. MediaPipe Framework API (Low-Level)

### 2.1 Core Concepts
| Concept | Description |
|---------|-------------|
| **Packet** | Immutable data container (type-erased, ref-counted) with optional timestamp |
| **Timestamp** | Monotonically increasing per stream; microseconds precision |
| **Stream** | Sequence of packets with same type, ordered by timestamp |
| **Calculator** | Processing node (GetContract, Open, Process, Close) |
| **Graph** | DAG of calculators (CalculatorGraphConfig protobuf) |
| **Subgraph** | Reusable graph module with defined I/O |

### 2.2 Key C++ Classes
```cpp
// Graph configuration (text protobuf)
CalculatorGraphConfig config = ParseTextProtoOrDie<CalculatorGraphConfig>(R"(
  input_stream: "in"
  output_stream: "out"
  node {
    calculator: "PassThroughCalculator"
    input_stream: "in"
    output_stream: "out1"
  }
  node {
    calculator: "PassThroughCalculator"
    input_stream: "out1"
    output_stream: "out"
  }
)");

// Main graph runner
CalculatorGraph graph;
MP_RETURN_IF_ERROR(graph.Initialize(config));

// Output polling (synchronous)
MP_ASSIGN_OR_RETURN(OutputStreamPoller poller, graph.AddOutputStreamPoller("out"));

// Or async callback
auto cb = [](const Packet& packet) -> absl::Status {
    auto value = packet.Get<std::string>();
    return absl::OkStatus();
};
MP_RETURN_IF_ERROR(graph.ObserveOutputStream("out", cb));

// Start
MP_RETURN_IF_ERROR(graph.StartRun({}));

// Send packets
for (int i = 0; i < 10; ++i) {
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream("in",
        MakePacket<std::string>("Hello World!").At(Timestamp(i))));
}
MP_RETURN_IF_ERROR(graph.CloseInputStream("in"));

// Poll results
Packet packet;
while (poller.Next(&packet)) {
    LOG(INFO) << packet.Get<std::string>();
}
MP_RETURN_IF_ERROR(graph.WaitUntilDone());
```

### 2.3 Image Processing
```cpp
// OpenCV Mat -> ImageFrame -> Packet
cv::Mat frame; // BGR
cv::cvtColor(frame, frameRGB, cv::COLOR_BGR2RGB);

auto inputFrame = absl::make_unique<ImageFrame>(
    ImageFormat::SRGB, frameRGB.cols, frameRGB.rows, ImageFrame::kDefaultAlignmentBoundary);
frameRGB.copyTo(formats::MatView(inputFrame.get()));

MP_RETURN_IF_ERROR(graph.AddPacketToInputStream("in",
    Adopt(inputFrame.release()).At(Timestamp(ts))));

// Packet -> ImageFrame -> cv::Mat
const ImageFrame& outputFrame = packet.Get<ImageFrame>();
cv::Mat outMat = formats::MatView(&outputFrame);
cv::cvtColor(outMat, outBGR, cv::COLOR_RGB2BGR);
```

### 2.4 Real-Time (FlowLimiterCalculator)
```protobuf
node {
  calculator: "FlowLimiterCalculator"
  input_stream: "image"
  output_stream: "throttled_image"
  input_stream: "FINISHED:finished"
  node_options: {
    [type.googleapis.com/mediapipe.FlowLimiterCalculatorOptions] {
      max_in_flight: 1
    }
  }
}
```

---

## 3. Building MediaPipe for openFrameworks

### 3.1 Challenge: Bazel Dependency
MediaPipe **requires Bazel** for building. No official CMake/pkg-config support.

### 3.2 Solution Options

#### Option A: libmediapipe (C wrapper, builds shared library)
- Repo: https://github.com/cpvrlab/libmediapipe
- Builds `libmediapipe.so` + headers via Bazel once
- Then use with CMake/Make in OF project
- Supports Linux (x86_64, aarch64), macOS, Windows, Android

```bash
# Build libmediapipe (one-time)
cd libmediapipe
./build-aarch64-linux.sh --version v0.10.14 --config release --opencv_dir /usr/local

# Output: libmediapipe-<version>-<arch>-<os>/
#   include/mediapipe/...    # Headers
#   lib/libmediapipe.so      # Shared library
#   data/                    # Protobuf configs, models
```

#### Option B: Build inside MediaPipe tree (Bazel-only)
- Copy OF project into `mediapipe/examples/desktop/`
- Use Bazel BUILD files
- Not compatible with standard OF workflow

#### Option C: MediaPipe Tasks C API (Simpler)
- Use C headers from `mediapipe/tasks/c/vision/*/`
- Link against prebuilt `libmediapipe_tasks.so` (if available)
- Still requires Bazel to build the library

### 3.3 openFrameworks addon_config.mk Template
```makefile
meta:
ADDON_NAME = ofxMediaPipe
ADDON_DESCRIPTION = MediaPipe integration for openFrameworks
ADDON_AUTHOR = 
ADDON_TAGS = "mediapipe" "ml" "vision" "pose" "face" "hands"
ADDON_URL = https://github.com/google/mediapipe

common:
# MediaPipe Tasks C API headers
ADDON_INCLUDES = libs/mediapipe/include

# Link against libmediapipe shared library
ADDON_LDFLAGS = -lmediapipe
ADDON_LDFLAGS += -lopencv_core -lopencv_imgproc -lopencv_videoio
ADDON_LDFLAGS += -labsl_strings -labsl_status -lprotobuf
ADDON_LDFLAGS += -ltensorflow-lite -lflatbuffers

# Model files copied to bin/data
ADDON_DATA = libs/mediapipe/models/*.task

linux64:
ADDON_LDFLAGS += -lmediapipe

linuxarmv6l:
ADDON_LDFLAGS += -lmediapipe

linuxarmv7l:
ADDON_LDFLAGS += -lmediapipe

# Platform-specific library paths
# ADDON_LIBS = libs/mediapipe/lib/linuxarmv7l/libmediapipe.so
```

---

## 4. Integration Architecture for ofxMediaPipe

### 4.1 Recommended Structure
```
ofxMediaPipe/
├── addon_config.mk
├── libs/
│   └── mediapipe/
│       ├── include/          # MediaPipe Tasks C headers
│       ├── lib/
│       │   ├── linuxarmv7l/  # libmediapipe.so for Pi
│       │   ├── linux64/
│       │   └── ...
│       └── models/           # .task model files
├── src/
│   ├── ofxMediaPipe.h
│   ├── ofxMediaPipe.cpp
│   ├── FaceLandmarker.h
│   ├── FaceLandmarker.cpp
│   ├── HandLandmarker.h
│   ├── HandLandmarker.cpp
│   ├── PoseLandmarker.h
│   └── PoseLandmarker.cpp
└── examples/
    ├── FaceMeshExample/
    ├── HandTrackingExample/
    └── PoseTrackingExample/
```

### 4.2 C++ Wrapper Design (ofxMediaPipe.h)
```cpp
#pragma once

#include "ofMain.h"
#include "mediapipe/tasks/c/vision/face_landmarker/face_landmarker.h"
#include "mediapipe/tasks/c/vision/hand_landmarker/hand_landmarker.h"
#include "mediapipe/tasks/c/vision/pose_landmarker/pose_landmarker.h"

namespace ofxMediaPipe {

enum class RunningMode { IMAGE, VIDEO, LIVE_STREAM };

struct FaceLandmark {
    glm::vec3 position;  // x, y normalized [0,1], z relative
    float visibility;
    float presence;
};

struct FaceLandmarkerResult {
    std::vector<std::vector<FaceLandmark>> multi_face_landmarks;
    std::vector<std::vector<float>> face_blendshapes;
    std::vector<glm::mat4> facial_transformation_matrices;
};

class FaceLandmarker {
public:
    struct Options {
        std::string model_path = "face_landmarker.task";
        RunningMode running_mode = RunningMode::LIVE_STREAM;
        int num_faces = 1;
        float min_detection_confidence = 0.5f;
        float min_presence_confidence = 0.5f;
        float min_tracking_confidence = 0.5f;
        bool output_blendshapes = false;
        bool output_transformation_matrices = false;
    };
    
    bool setup(const Options& options);
    void update(ofPixels& pixels);  // LIVE_STREAM mode
    FaceLandmarkerResult detect(ofPixels& pixels);  // IMAGE mode
    FaceLandmarkerResult detectForVideo(ofPixels& pixels, int64_t timestamp_ms);  // VIDEO mode
    void close();
    bool isInitialized() const;
    
    // Callback for LIVE_STREAM mode
    std::function<void(const FaceLandmarkerResult&, const ofPixels&, int64_t)> onResult;
    
private:
    MpFaceLandmarkerPtr landmarker_ = nullptr;
    Options options_;
};

} // namespace ofxMediaPipe
```

### 4.3 Implementation Notes
- Convert `ofPixels` (RGB) to `MpImage` (SRGB)
- Handle async callback thread safety (dispatch to main thread)
- Manage model file paths via `ofToDataPath()`
- Use `ofxCv` for OpenCV interop if needed

---

## 5. Raspberry Pi (ARM64) Specifics

### 5.1 Build libmediapipe for aarch64
```bash
# On Pi or cross-compile
cd libmediapipe
./build-aarch64-linux.sh --version v0.10.14 --config release --opencv_dir /usr/local
```

### 5.2 Dependencies (apt)
```bash
sudo apt install -y \
    libopencv-dev \
    libabsl-dev \
    libprotobuf-dev \
    libflatbuffers-dev \
    libtensorflow-lite-dev \
    bazelisk  # for building libmediapipe
```

### 5.3 Performance Notes
- Pi 4/5: Use `ComplexityLite` (model_complexity=0) for real-time
- Enable GPU via OpenGL ES (MediaPipe GPU delegate)
- Expect 15-30 FPS for Pose (Lite), 30-60 FPS for Face/Hands

---

## 6. Key Documentation Links

| Topic | URL |
|-------|-----|
| MediaPipe Framework C++ | https://developers.google.com/edge/mediapipe/framework/getting_started/cpp |
| Hello World C++ | https://developers.google.com/edge/mediapipe/framework/getting_started/hello_world_cpp |
| Calculator Concepts | https://developers.google.com/edge/mediapipe/framework/framework_concepts/calculators |
| Graph Concepts | https://developers.google.com/edge/mediapipe/framework/framework_concepts/graphs |
| MediaPipe Tasks C API | https://github.com/google-ai-edge/mediapipe/tree/master/mediapipe/tasks/c/vision |
| libmediapipe (shared lib) | https://github.com/cpvrlab/libmediapipe |
| IT-JIM Tutorial | https://www.it-jim.com/blog/mini-tutorial-on-mediapipe/ |
| First Steps Repo | https://github.com/agrechnev/first_steps_mediapipe |
| MediaPipe Visualizer | https://viz.mediapipe.dev/ |
| Solutions Guide | https://developers.google.com/edge/mediapipe/solutions/guide |

---

## 7. Legacy vs New Solutions

| Legacy (Deprecated) | New (MediaPipe Tasks) |
|---------------------|----------------------|
| Face Mesh | Face Landmarker |
| Hands | Hand Landmarker |
| Pose | Pose Landmarker |
| Holistic | Holistic Landmarker |
| Face Detection | Face Detector |
| Selfie/Hair Segmentation | Image Segmenter |
| Object Detection | Object Detector |

**Use MediaPipe Tasks API** - simpler, maintained, cross-platform.

---

## 8. Example Graph Configs (for Framework API)

### Face Mesh (CPU)
```
mediapipe/graphs/face_mesh/face_mesh_desktop_live.pbtxt
```

### Face Mesh (GPU)
```
mediapipe/graphs/face_mesh/face_mesh_desktop_live_gpu.pbtxt
```

### Hand Tracking (CPU)
```
mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt
```

### Pose Tracking (CPU)
```
mediapipe/graphs/pose_tracking/pose_tracking_cpu.pbtxt
```

### Pose Tracking (GPU)
```
mediapipe/graphs/pose_tracking/pose_tracking_gpu.pbtxt
```

---

## 9. Next Steps for ofxMediaPipe

1. **Build libmediapipe** for ARM64 (Raspberry Pi)
2. **Copy headers + .so** to `libs/mediapipe/`
3. **Download .task models** to `libs/mediapipe/models/`
4. **Write C++ wrappers** for Face/Hand/Pose landmarkers
5. **Create addon_config.mk** with proper flags
6. **Build examples** using OF Project Generator
7. **Test on Pi** with USB camera

---

## References (from RESEARCH.md)

1. https://www.it-jim.com/blog/mini-tutorial-on-mediapipe/
2. https://github.com/agrechnev/first_steps_mediapipe
3. https://chuoling.github.io/mediapipe/getting_started/cpp.html
4. https://chuoling.github.io/mediapipe/getting_started/hello_world_cpp.html
5. https://developers.google.com/edge/mediapipe/framework/getting_started/hello_world_cpp
6. https://developers.google.com/edge/mediapipe/framework/getting_started/cpp
7. https://github.com/cpvrlab/libmediapipe
8. https://developers.google.com/edge/mediapipe/solutions/guide
9. MediaPipe Tasks C++ headers (face/hand/pose landmarker)
10. openFrameworks addon_config.mk specification