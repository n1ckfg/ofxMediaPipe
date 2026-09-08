# example_gesture

Gesture recognition on one feed.

Each hand is drawn as a 21-point skeleton tinted by whichever gesture was
recognized, labelled with the gesture, its confidence, and which hand it is. The
window background also reacts to the winning gesture, to show the result being
*used* rather than just printed.

For the body skeleton, see `example_pose`; for both models at once, see
`apps/myApps/MediaPipeExample`.

## Build and run

```bash
make -j3
make run
```

`bin/data/gesture_recognizer.task` must be present; it is already copied here.
The pose model is not needed.

## The gestures

From the trained classifier inside `gesture_recognizer.task` — not hand-written
finger-angle heuristics:

`None`, `Closed_Fist`, `Open_Palm`, `Pointing_Up`, `Thumb_Down`, `Thumb_Up`,
`Victory`, `ILoveYou`.

`None` means a hand was found but matched no known gesture, so this example
ignores it when picking a winner. `Settings::minGestureScore` sets how confident
the classifier must be before a label is reported at all.

## What it shows

- Running one model on the tracker's worker thread. `Settings::enablePose` is
  left `false`, so that model is never loaded and costs nothing per pass: a pass
  is gesture time alone (~240 ms on a Raspberry Pi 4, so roughly 4 passes/sec —
  the HUD reports it).
- Reading `Hand::gesture` and `Hand::handedness` as `ofxMediaPipe::Category`,
  including the confidence score.
- Colouring each hand independently, so two hands showing different gestures
  read differently.
- Acting on a recognition result, with the change eased so that a single
  misclassified frame does not strobe the window.

Handedness is reported from the subject's point of view, not the camera's. Note
that the frame handed to the tracker is never mirrored, even when the display
is: flipping the image would swap those `Left`/`Right` labels.

## Video input

Same as `example_pose`: `ofVideoGrabber` when `/dev/video0` exists, otherwise the
first image in `bin/data`. See `apps/myApps/MediaPipeExample` for CSI camera,
movie-file and synthetic sources with auto-detection.

## Controls

| Key | Action |
|---|---|
| `m` | toggle mirroring |
| `f` | fullscreen |
