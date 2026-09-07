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
