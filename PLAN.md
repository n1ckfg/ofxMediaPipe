# ofxMediaPipe Status

## Working

- MediaPipe Tasks Vision **C API** wrapped for pose landmarking and gesture
  recognition, built as a single `libmediapipe_tasks_vision.so`.
- `scripts/build_mediapipe.sh` builds and installs the library, headers and
  models end to end.
- `ofxMediaPipe::Tracker` runs both models on a worker thread.
- `examples/example_pose` — pose tracking on its own.
- `examples/example_gesture` — pose tracking and gesture recognition together.
- `apps/myApps/MediaPipeExample` — the fuller app, with video-source auto-detection.

Verified on a Raspberry Pi 4 (aarch64, GCC 12, OpenCV 4.6, MediaPipe v0.10.35):
both models load and detect correctly, checked against MediaPipe's own test
images (`thumb_up.jpg` -> `Thumb_Up`, `victory.jpg` -> `Victory`,
`fist.jpg` -> `Closed_Fist`, plus 33/21 landmark counts).

## Not implemented

The C API also ships face landmarking, hand landmarking on its own, object
detection, image classification, image embedding and segmentation. Each would
follow the same shape as `ofxMediaPipePoseLandmarker.*`: add the task's
`*_c_lib` to the Bazel target in `build_mediapipe.sh`, then wrap it.

Only the CPU delegate is wired up. `BaseOptions::delegate` also accepts `GPU`,
which MediaPipe implements over OpenGL ES, but the library is built with
`--define MEDIAPIPE_DISABLE_GPU=1`.

`PoseLandmarkerOptions::output_segmentation_masks` is hardcoded off; the result
struct's `segmentation_masks` field is consequently never read.

## History

Earlier revisions of this addon targeted the MediaPipe Tasks **C++** API and a
`libmediapipe.so` from cpvrlab/libmediapipe. Both were dead ends: the C++ API
would have forced protobuf and absl into every dependent app's build, and
cpvrlab/libmediapipe pins MediaPipe v0.8.11, which predates the Tasks API
entirely (it has no gesture recognizer, and no aarch64 Linux build script).
See ARCHITECTURE.md for the reasoning behind the current approach.
