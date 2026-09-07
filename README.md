# ofxMediaPipe

openFrameworks bindings for Google MediaPipe's **Tasks Vision C API**, covering
pose landmarking and gesture recognition.

## What this gives you

| Class | Wraps | Produces |
|---|---|---|
| `ofxMediaPipe::PoseLandmarker` | `MpPoseLandmarker*` | 33 body landmarks per pose, normalized and in world (metric) space |
| `ofxMediaPipe::GestureRecognizer` | `MpGestureRecognizer*` | 21 hand landmarks per hand, plus a gesture label and handedness |
| `ofxMediaPipe::Tracker` | both of the above | the same results, computed on a worker thread |
| `ofxMediaPipe::drawPose` / `drawHand` | — | skeleton overlays |

The gesture labels are the trained canned-gesture classes that ship inside
`gesture_recognizer.task`: `None`, `Closed_Fist`, `Open_Palm`, `Pointing_Up`,
`Thumb_Down`, `Thumb_Up`, `Victory`, `ILoveYou`.

## Setup

MediaPipe only builds under Bazel, which openFrameworks' Make-based build cannot
drive. So MediaPipe is built once, ahead of time, into a shared library that the
addon then links as a prebuilt dependency:

```bash
cd addons/ofxMediaPipe
./scripts/build_mediapipe.sh
```

The script fetches Bazel and the MediaPipe sources, points MediaPipe at your
system OpenCV, builds the library, then installs the library, its headers and
the two `.task` models into `libs/mediapipe/`. Options:

```
--version v0.10.35   MediaPipe tag to build
--jobs 3             parallel Bazel jobs (lower this if you run out of RAM)
--keep-src           keep the Bazel cache and sources afterwards
```

Requirements: `git`, `curl`, `python3` with `numpy`, `g++`, and OpenCV 4
(`apt install libopencv-dev python3-numpy`). Budget a few hours and ~20 GB of
free disk on a Raspberry Pi 4; 8 GB of RAM is comfortable at `--jobs 3`.

Then copy the models next to your app:

```bash
cp libs/mediapipe/models/*.task ../../apps/myApps/YourApp/bin/data/
```

## Usage

```cpp
#include "ofxMediaPipe.h"

ofxMediaPipe::Tracker tracker;
ofxMediaPipe::Tracker::Results results;

void ofApp::setup() {
    ofxMediaPipe::Tracker::Settings settings;
    settings.pose.modelPath = "pose_landmarker_lite.task";
    settings.gesture.modelPath = "gesture_recognizer.task";
    settings.gesture.numHands = 2;
    settings.inferenceWidth = 256;   // downscale before inference
    tracker.setup(settings);         // models load on the worker thread
}

void ofApp::update() {
    if (grabber.isFrameNew()) {
        tracker.setPixels(grabber.getPixels());
    }
    tracker.getResults(results);     // false when nothing new finished
}

void ofApp::draw() {
    ofRectangle bounds(0, 0, ofGetWidth(), ofGetHeight());
    for (const auto & pose : results.poses) {
        ofxMediaPipe::drawPose(pose, bounds);
    }
    for (const auto & hand : results.hands) {
        ofxMediaPipe::drawHand(hand, bounds);
        ofDrawBitmapString(hand.gesture.categoryName,
                           ofxMediaPipe::toScreen(hand.landmarks[0], bounds));
    }
}
```

Landmark positions are normalized to [0,1] over the input image, so they map
onto whatever rectangle you drew the frame into, at any scale.

`Tracker::setup()` returns immediately and loads the models on its worker
thread; poll `isReady()` and `isFailed()` / `getError()` for the outcome.

The synchronous `PoseLandmarker` and `GestureRecognizer` classes are available
if you want to drive inference yourself. Both run in MediaPipe's VIDEO mode,
which requires strictly increasing timestamps:

```cpp
ofxMediaPipe::PoseLandmarker pose;
pose.setup();
std::vector<ofxMediaPipe::Pose> poses;
pose.detect(pixels, ++timestampMs, poses);
```

Call these from one thread only — a MediaPipe task in VIDEO mode keeps tracking
state between frames and is not safe to drive concurrently.

## Design notes

**One shared library, not two.** Upstream ships a separate `.so` per task.
Loading both would put two private copies of the MediaPipe, absl and TFLite
runtimes into a single process, so `build_mediapipe.sh` adds a Bazel target that
links both C APIs into one `libmediapipe_tasks_vision.so`.

**The C API, not the C++ one.** MediaPipe's C++ Tasks API would drag protobuf,
absl and the whole MediaPipe header tree into your app's compile and link. The C
API is a flat, stable surface that hides all of that behind one `.so`.

**MediaPipe headers stay out of the public headers.** They are included only
from the addon's `.cpp` files, so including `ofxMediaPipe.h` does not pull
MediaPipe's global-scope `RunningMode` enum into your app.

## Layout

```
ofxMediaPipe/
├── addon_config.mk
├── scripts/build_mediapipe.sh    # builds + installs everything below libs/
├── src/
│   ├── ofxMediaPipe.h            # umbrella header
│   ├── ofxMediaPipeTypes.*       # Landmark, Category, Pose, Hand, skeletons
│   ├── ofxMediaPipePoseLandmarker.*
│   ├── ofxMediaPipeGestureRecognizer.*
│   ├── ofxMediaPipeTracker.*     # threaded, runs both models
│   ├── ofxMediaPipeDraw.*
│   └── ofxMediaPipeInternal.*    # C API <-> addon type conversion (private)
└── libs/mediapipe/
    ├── include/mediapipe/tasks/c/...
    ├── lib/<platform>/libmediapipe_tasks_vision.so
    └── models/*.task
```

## Example

`apps/myApps/MediaPipeExample` runs both models on a live feed and auto-detects
its video source (Pi camera, USB webcam, movie file, still image, or synthetic).
