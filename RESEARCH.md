# ofxMediaPipe Research Notes

Findings from getting MediaPipe running under openFrameworks on a Raspberry Pi 4
(aarch64, Raspberry Pi OS bookworm, GCC 12, OpenCV 4.6). Verified against
MediaPipe **v0.10.35** unless noted.

For *why* the addon is shaped the way it is, see ARCHITECTURE.md. This file is
the background: what was checked, what was measured, and which routes are dead
ends.

---

## 1. Which API

MediaPipe exposes vision three ways.

| API | Path | Notes |
|---|---|---|
| Framework | `CalculatorGraph` + `.pbtxt` | The pre-Tasks way. Still present, but there is no gesture *model* here — the old demos derive gestures from hand landmarks in user code. |
| Tasks C++ | `mediapipe/tasks/cc/vision/...` | Real C++ API, but every dependent app inherits protobuf, absl and the MediaPipe header tree at compile *and* link time. |
| Tasks C | `mediapipe/tasks/c/vision/...` | Flat `extern "C"` surface, one `.so`, no transitive headers. **What this addon uses.** |

The Tasks C API is official and complete. As of v0.10.35 it covers:

```
face_detector      face_landmarker    gesture_recognizer   hand_landmarker
holistic_landmarker image_classifier  image_embedder       image_segmenter
interactive_segmenter  object_detector  pose_landmarker
```

Each ships a `cc_binary(linkshared = True)` target, e.g.
`//mediapipe/tasks/c/vision/pose_landmarker:libpose_landmarker.so`.

**Important:** no custom Bazel target is needed to get a gesture recognizer.
That is a common misconception (it was this project's own starting assumption).
The trained canned-gesture classifier is right there in the C API.

### Canned gestures

`gesture_recognizer.task` classifies into eight categories:

```
None  Closed_Fist  Open_Palm  Pointing_Up  Thumb_Down  Thumb_Up  Victory  ILoveYou
```

`None` means a hand was found but matched nothing. A second, empty "custom
gestures" classifier slot exists for models trained with MediaPipe Model Maker.

---

## 2. Dead ends

### cpvrlab/libmediapipe — do not use for this

Widely cited (including by this addon's own first draft) as the way to get a
MediaPipe shared library. It is not viable here:

- It pins MediaPipe **v0.8.11**, which *predates the Tasks API entirely*. No
  `.task` bundles, no gesture recognizer.
- Its C API is graph-generic — `mp_create_instance_builder`, `mp_poll_packet`,
  `mp_get_norm_multi_face_landmarks` — not task-shaped.
- **There is no `build-aarch64-linux.sh`.** The repo ships build scripts for
  x86_64 Linux/Windows/macOS, aarch64 macOS and aarch64 Android only. Docs
  quoting that filename (this addon's included, previously) are wrong.
- Its Linux script requires a `bazel-5.2.0` binary on `PATH`.

Building the Tasks C API directly from the MediaPipe tree is both simpler and
gets a far newer MediaPipe.

### System packages don't help

`libabsl-dev`, `libprotobuf-dev`, `libflatbuffers-dev`, `libtensorflow-lite-dev`
are **not** needed and should not be linked against. Bazel builds and statically
links its own pinned copies of all of them into the `.so`. The only shared
libraries the result needs are OpenCV, libstdc++, libm and libc:

```
$ readelf -d libmediapipe_tasks_vision.so | grep NEEDED
libopencv_core.so.406, libopencv_calib3d.so.406, libopencv_features2d.so.406,
libopencv_highgui.so.406, libopencv_imgcodecs.so.406, libopencv_imgproc.so.406,
libopencv_video.so.406, libopencv_videoio.so.406,
libm.so.6, libstdc++.so.6, libgcc_s.so.1, libc.so.6
```

Likewise Debian's `bazel` package is only a wrapper script that dispatches to a
real Bazel binary and errors out if none matches; it is not itself Bazel.

---

## 3. Build gotchas

Both are patched by `scripts/build_mediapipe.sh`; see ARCHITECTURE.md for the
detail.

1. **OpenCV 4 paths are commented out** in `third_party/opencv_linux.BUILD`,
   which targets the OpenCV 3 layout.
2. **`CompileTimeString` breaks under GCC.** `framework/api3/node.h` uses it as a
   class-type non-type template parameter while its copy constructor is deleted;
   GCC requires copy-constructibility there, Clang does not. This kills the build
   at ~3100/3182 actions, hours in, on any calculator using `api3::Node`.

Other practical notes:

- Bazel version comes from `.bazelversion` (7.4.1 at v0.10.35). Honour it.
- Build with `--define MEDIAPIPE_DISABLE_GPU=1` unless you are wiring up the
  OpenGL ES delegate.
- `--jobs=3` on a 4-core, 8 GB Pi 4 keeps memory in bounds. Budget ~2-3 hours
  cold, and ~20 GB of disk for the Bazel cache.
- Linking two per-task `.so` files into one process gives two private copies of
  the MediaPipe/absl/TFLite runtime; link one combined library instead.

---

## 4. Measured performance

Raspberry Pi 4 Model B, CPU delegate, `pose_landmarker_lite`, frames downscaled
to 256px wide:

| Workload | Per frame | Rate |
|---|---|---|
| Pose only | ~115 ms | ~8-9 /sec |
| Pose + gesture | ~115 + ~240 ms | ~2-3 /sec |

Two corrections to figures that circulate widely:

- **"15-30 FPS for Pose Lite on a Pi 4" is not achievable on CPU.** Measured is
  roughly a third of the bottom of that range. Sources quoting it are usually
  describing a GPU delegate, a desktop, or a Pi 5.
- Model *load* takes several seconds — long enough that code timing anything
  from application start rather than from a readiness flag will race it.

Inference must therefore run off the draw thread, which is what
`ofxMediaPipe::Tracker` exists to do.

---

## 5. API shape worth knowing

- **VIDEO mode requires strictly increasing timestamps** and keeps tracking state
  between frames. Two consequences: drive one task from one thread only, and
  expect a single cold-start pass on a still image to score *worse* than the same
  image fed for a few frames.
- **Results are parallel arrays.** `GestureRecognizerResult` carries `gestures`,
  `handedness`, `hand_landmarks` and `hand_world_landmarks` as separate arrays
  indexed in step, each with its own count — check every count before indexing.
- **Categories arrive sorted best-first**, so `categories[0]` is the winner.
- **`ClassifierOptions::max_results` has no default and 0 is rejected.** Set it
  explicitly for both the canned and custom gesture classifiers, or creation
  fails.
- **Landmarks are normalized to [0,1]** over the input image, so inference can
  run on a downscaled frame and still map onto the full-size one.
- **Handedness is from the subject's point of view.** Feeding a mirrored frame
  swaps `Left` and `Right`.
- Images must be RGB or RGBA (`kMpImageFormatSrgb` / `kMpImageFormatSrgba`).

---

## 6. Raspberry Pi camera reality

Relevant to any app feeding this addon, and the reason `MediaPipeExample` carries
its own video source:

- `ofVideoGrabber` **cannot open a CSI camera.** It goes through V4L2/GStreamer;
  the ribbon-cable camera is behind libcamera.
- Most `/dev/video*` nodes on a Pi are `bcm2835-codec` and `bcm2835-isp`
  memory-to-memory devices, not capture devices. They advertise
  `V4L2_CAP_VIDEO_CAPTURE`, so filter on `V4L2_CAP_VIDEO_M2M` and the driver name
  before trying to open one.
- A stock Raspberry Pi OS has **no `libcamerasrc` GStreamer plugin** (no
  `libgstlibcamera.so` under `gstreamer-1.0/`), so that route is unavailable by
  default. `rpicam-vid` is installed and can stream raw frames over a pipe, which
  is what `MediaPipeExample` does.

  Caveat: that CSI path is **written but unverified** — no camera was attached to
  the machine this was developed on, so everything else here was tested and it
  was not. It requests I420 and rounds the size up to a multiple of 32x16 on the
  assumption that `rpicam-vid` then emits tightly packed planes; if the picture
  comes out skewed or sheared, that assumption is where to look, as it would mean
  the planes carry row padding the reader does not account for.

---

## 7. Reference links

| Topic | URL |
|---|---|
| Tasks C API source | https://github.com/google-ai-edge/mediapipe/tree/master/mediapipe/tasks/c/vision |
| Pose Landmarker docs + models | https://ai.google.dev/edge/mediapipe/solutions/vision/pose_landmarker |
| Gesture Recognizer docs + models | https://ai.google.dev/edge/mediapipe/solutions/vision/gesture_recognizer |
| Model Maker (custom gestures) | https://ai.google.dev/edge/mediapipe/solutions/customization/gesture_recognizer |
| Solutions guide | https://ai.google.dev/edge/mediapipe/solutions/guide |
| cpvrlab/libmediapipe (see §2) | https://github.com/cpvrlab/libmediapipe |

Model downloads used by `scripts/build_mediapipe.sh`:

```
https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/latest/pose_landmarker_lite.task
https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/latest/gesture_recognizer.task
```
