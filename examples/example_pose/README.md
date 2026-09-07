# example_pose

Basic pose tracking: one body, 33 landmarks, drawn over the video feed.

## Build and run

```bash
make -j3
make run
```

`bin/data/pose_landmarker_lite.task` must be present — `scripts/build_mediapipe.sh`
downloads it into the addon and it is already copied here.

## What it shows

- Loading the pose model through `ofxMediaPipe::Tracker` with the gesture model
  switched off, so the worker spends all its time on pose.
- Mapping normalized landmarks onto the drawn frame with `ofxMediaPipe::toScreen`.
- Drawing a skeleton with `ofxMediaPipe::drawPose`.
- Reading individual landmarks by name through `PoseLandmarkIndex` — the nose and
  both wrists are circled and labelled — and skipping ones the model reports as
  occluded via `Landmark::visibility`.

## Why the tracker instead of PoseLandmarker directly

A pose pass costs ~115 ms on a Raspberry Pi 4, far more than a frame's budget.
Calling `PoseLandmarker::detect()` inline in `update()` would drag the whole app
down to the model's rate. `Tracker` moves that onto a worker thread, so drawing
stays smooth and results simply arrive when they are ready.

`PoseLandmarker` is still available if you would rather own the threading.

## Video input

Uses `ofVideoGrabber` if `/dev/video0` exists, and otherwise falls back to the
first image it finds in `bin/data` — so drop a photo of a person in there to try
it without a camera.

That fallback is deliberately minimal. A Raspberry Pi's CSI camera is not
reachable through `ofVideoGrabber` at all, and most of its `/dev/video*` nodes
are codec and ISP devices rather than capture devices. For CSI, movie-file and
synthetic sources with auto-detection, see `apps/myApps/MediaPipeExample`.

## Controls

| Key | Action |
|---|---|
| `m` | toggle mirroring |
| `l` | toggle landmark labels |
| `f` | fullscreen |
