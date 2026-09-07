#pragma once

#include "ofMain.h"
#include "ofxMediaPipeTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace ofxMediaPipe {

/// Wraps the MediaPipe Tasks PoseLandmarker C API in VIDEO running mode.
///
/// The MediaPipe C headers are kept out of this header so that including it
/// does not pull MediaPipe's global-scope `RunningMode` enum into your app.
class PoseLandmarker {
public:
	struct Settings {
		/// Resolved with ofToDataPath() if relative.
		std::string modelPath = "pose_landmarker_lite.task";
		int numPoses = 1;
		float minDetectionConfidence = 0.5f;
		float minPresenceConfidence = 0.5f;
		float minTrackingConfidence = 0.5f;
	};

	PoseLandmarker();
	~PoseLandmarker();

	// Holds an opaque C handle; copying would double-free it.
	PoseLandmarker(const PoseLandmarker &) = delete;
	PoseLandmarker & operator=(const PoseLandmarker &) = delete;

	/// Loads the model. The no-argument form uses Settings' defaults.
	bool setup();
	bool setup(const Settings & settings);
	void close();
	bool isSetup() const;

	/// Runs detection on one frame. `timestampMs` must increase strictly
	/// monotonically between calls or MediaPipe rejects the frame.
	/// Returns false and leaves `outPoses` untouched on failure.
	bool detect(const ofPixels & pixels, uint64_t timestampMs, std::vector<Pose> & outPoses);

	/// Message from the most recent failure, or "" if the last call succeeded.
	const std::string & getError() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace ofxMediaPipe
