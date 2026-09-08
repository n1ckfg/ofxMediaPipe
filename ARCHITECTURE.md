# ofxMediaPipe Architecture

## The build problem

MediaPipe builds only under Bazel. openFrameworks builds under Make. Nothing
reconciles those two, so the addon does not try: MediaPipe is compiled once,
ahead of time, into a shared library, and the addon links that as a prebuilt
dependency. `scripts/build_mediapipe.sh` owns that step.

## Which MediaPipe API

MediaPipe exposes its vision tasks three ways, and the choice matters:

| API | Verdict |
|---|---|
| Framework (`CalculatorGraph`, `.pbtxt`) | Legacy. Gesture recognition would have to be hand-written from hand landmarks. |
| Tasks **C++** (`mediapipe/tasks/cc/...`) | Pulls protobuf, absl and the MediaPipe header tree into the app's compile *and* link. |
| Tasks **C** (`mediapipe/tasks/c/...`) | Flat, stable, hides everything behind one `.so`. **Chosen.** |

The C API also ships the trained canned-gesture classifier, so gesture
recognition is a real model rather than finger-angle heuristics.

## One shared library

Upstream defines a separate `cc_binary(linkshared)` per task —
`libpose_landmarker.so`, `libgesture_recognizer.so`. Each statically links the
whole MediaPipe, absl and TFLite runtime. Loading both into one process means
two private copies of that runtime in one address space.

So the build script adds its own Bazel target,
`//mediapipe/tasks/c/vision/ofx:libmediapipe_tasks_vision.so`, depending on the
`*_c_lib` targets of both tasks (they are marked `alwayslink = 1`, so their
symbols survive). One library, one runtime.

## What the build patches, and why

MediaPipe's tree does not build unmodified against a Debian/Raspberry Pi OS
toolchain. `scripts/build_mediapipe.sh` applies two patches; both are upstream
portability gaps, not local preferences.

**`third_party/opencv_linux.BUILD` — OpenCV 4 paths.** The file targets the
OpenCV 3 layout and ships with the OpenCV 4 header globs commented out. Debian
puts OpenCV 4 headers in `/usr/include/opencv4`, with `cvconfig.h` under an
arch-specific directory, so the script uncomments those lines and substitutes
the real arch triplet from `gcc -dumpmachine`.

**`framework/deps/compile_time_string.h` — GCC vs Clang.** `framework/api3/node.h`
takes a `CompileTimeString` as a *class-type non-type template parameter*:

```cpp
template <CompileTimeString kRegistrationName>
struct Node { ... };
```

GCC requires such a parameter object to be copy-constructible. Clang, which
Google builds with, does not. `CompileTimeString` deletes its copy constructor,
so under GCC every calculator built on `api3::Node` fails to compile — including
`hand_landmarks_deduplication_calculator`, which the gesture recognizer needs.
The script defaults the copy constructor instead of deleting it. That is safe:
every member is `const`, so the copy is trivial, the type stays structural, and
assignment remains deleted.

Without this patch the build dies around 3100 of 3182 actions, after roughly two
hours of work — so it is worth knowing about before starting.

**Bazel version.** MediaPipe pins its own Bazel through `.bazelversion` (7.4.1
at v0.10.35). The script downloads exactly that release rather than relying on
whatever `bazel` resolves to on the host; Debian's `/usr/bin/bazel` is a wrapper
that fails outright if no matching version is installed.

## Layers

```
        ofApp
          |
   Tracker (ofThread)          <- owns the worker thread and frame hand-off
          |
   PoseLandmarker / GestureRecognizer   <- one task each, synchronous
          |
   internal::  makeImage, toLandmarks, toCategory, ScopedImage
          |
   MediaPipe Tasks Vision C API  (libmediapipe_tasks_vision.so)
```

### Type translation

`internal::` converts between MediaPipe's C structs and the addon's types:
`NormalizedLandmark`/`Landmark` become `ofxMediaPipe::Landmark` (a `glm::vec3`
plus visibility and presence), and `Category` becomes `ofxMediaPipe::Category`.

`ofPixels` becomes an `MpImage` in `internal::makeImage`. MediaPipe accepts only
RGB and RGBA, so any other pixel format is converted once into a scratch buffer
that persists across frames. `ScopedImage` owns the resulting handle so it is
released on every return path, including the error ones.

### Threading

Measured on a Raspberry Pi 4 (CPU delegate, frames downscaled to 256px): pose
~115 ms, gesture ~240 ms. Running both is therefore ~350 ms per pass, or 2-3
passes per second — an order of magnitude slower than a 60 Hz draw loop, which
is the whole reason inference cannot sit in `update()`.

`Tracker` runs both models on one worker thread. Two properties matter:

- **All MediaPipe calls for a task happen on the thread that created it.** The
  models are therefore loaded inside `threadedFunction()`, not in `setup()`.
  `setup()` returns immediately; `isReady()` and `isFailed()` report the result.
- **Frames are dropped, not queued.** `setPixels()` overwrites any frame the
  worker has not yet picked up. Queueing would make results fall progressively
  further behind the live camera, since inference is slower than capture.

`stop()` sets the stop flag and then signals the input condition variable, so a
worker parked waiting for a frame wakes up to observe the request rather than
blocking until the next frame arrives.

### Timestamps

Both tasks run in MediaPipe's VIDEO mode, which keeps tracking state between
frames and rejects any timestamp that does not strictly increase. `Tracker`
keeps its own monotonic counter rather than trusting frame arrival times.

## Public surface

MediaPipe's headers are included only from the addon's `.cpp` files; the task
classes hold their C handles through a pimpl. Including `ofxMediaPipe.h`
therefore does not drop MediaPipe's global-scope `RunningMode` enum, or its
`Landmark` and `Category` structs, into the app's global namespace — which
matters, since the addon defines its own types by those names.

## Consumers

Three programs exercise this, in increasing order of complexity:

| Where | Uses |
|---|---|
| `examples/example_pose` | `Tracker` with gesture disabled; `drawPose`; landmark lookup by `PoseLandmarkIndex`. |
| `examples/example_gesture` | `Tracker` with pose disabled; `Hand::gesture` and `Hand::handedness`; per-hand tinting. |
| `apps/myApps/MediaPipeExample` | The above, plus a `VideoSource` that auto-detects a Pi CSI camera, a USB webcam, a movie, a still, or synthetic frames. |

The examples take video from `ofVideoGrabber` with a still-image fallback, which
keeps them focused on the addon's API. Capture on a Raspberry Pi is its own
problem — a CSI camera is unreachable through `ofVideoGrabber`, and most
`/dev/video*` nodes there are codec and ISP devices rather than capture devices
— so that work lives in the application rather than in the addon or the
examples.
