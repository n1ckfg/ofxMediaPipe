#pragma once

#include "ofMain.h"
#include "ofxMediaPipeTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace ofxMediaPipe {

/// Wraps the MediaPipe Tasks GestureRecognizer C API in VIDEO running mode.
///
/// This is the trained canned-gesture classifier that ships inside
/// gesture_recognizer.task; it reports one label per detected hand alongside
/// the 21 hand landmarks, so there is no need to hand-roll finger heuristics.
class GestureRecognizer {
public:
	struct Settings {
		/// Resolved with ofToDataPath() if relative.
		std::string modelPath = "gesture_recognizer.task";
		int numHands = 2;
		float minDetectionConfidence = 0.5f;
		float minPresenceConfidence = 0.5f;
		float minTrackingConfidence = 0.5f;
		/// Gestures scoring below this are reported as "None".
		float minGestureScore = 0.f;
	};

	GestureRecognizer();
	~GestureRecognizer();

	GestureRecognizer(const GestureRecognizer &) = delete;
	GestureRecognizer & operator=(const GestureRecognizer &) = delete;

	/// Loads the model. The no-argument form uses Settings' defaults.
	bool setup();
	bool setup(const Settings & settings);
	void close();
	bool isSetup() const;

	/// Runs recognition on one frame. `timestampMs` must increase strictly
	/// monotonically between calls.
	/// Returns false and leaves `outHands` untouched on failure.
	bool recognize(const ofPixels & pixels, uint64_t timestampMs, std::vector<Hand> & outHands);

	const std::string & getError() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace ofxMediaPipe
