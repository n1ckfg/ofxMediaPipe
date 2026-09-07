#pragma once

#include "ofMain.h"
#include "ofxMediaPipeGestureRecognizer.h"
#include "ofxMediaPipePoseLandmarker.h"
#include "ofxMediaPipeTypes.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

namespace ofxMediaPipe {

/// Runs pose landmarking and gesture recognition on a worker thread.
///
/// Both models together cost far more than a frame's budget on a Raspberry Pi,
/// so inference must not sit in update(). Submit frames with `setPixels()` and
/// poll with `getResults()`; frames that arrive while the worker is busy are
/// dropped rather than queued, which keeps results tracking the live camera
/// instead of falling progressively further behind.
class Tracker : public ofThread {
public:
	struct Settings {
		PoseLandmarker::Settings pose;
		GestureRecognizer::Settings gesture;

		bool enablePose = true;
		bool enableGesture = true;

		/// Width to downscale to before inference; 0 keeps the source size.
		/// Landmarks are normalized, so results stay valid at any scale.
		int inferenceWidth = 256;
	};

	/// One completed inference pass.
	struct Results {
		std::vector<Pose> poses;
		std::vector<Hand> hands;
		uint64_t timestampMs = 0;
		/// Wall-clock cost of the pass, for on-screen diagnostics.
		float poseMs = 0.f;
		float gestureMs = 0.f;
	};

	Tracker() = default;
	~Tracker() override;

	/// Starts the worker. The models are loaded on the worker thread, so this
	/// returns immediately; poll `isReady()` / `getError()` for the outcome.
	void setup(const Settings & settings);

	/// Stops the worker and releases the models. Safe to call more than once.
	void stop();

	/// True once both enabled models have loaded successfully.
	bool isReady() const { return ready.load(); }
	/// True if the worker gave up during model loading.
	bool isFailed() const { return failed.load(); }
	std::string getError() const;

	/// Hands a frame to the worker, replacing any frame not yet picked up.
	void setPixels(const ofPixels & pixels);

	/// Copies the newest results out. Returns false if nothing new has
	/// completed since the previous call.
	bool getResults(Results & out);

	/// Completed inference passes per second.
	float getInferenceFps() const { return inferenceFps.load(); }

private:
	void threadedFunction() override;

	Settings settings;

	PoseLandmarker pose;
	GestureRecognizer gesture;

	std::atomic<bool> ready {false};
	std::atomic<bool> failed {false};
	std::atomic<float> inferenceFps {0.f};

	mutable std::mutex errorMutex;
	std::string error;

	// Input hand-off: `input` holds at most one pending frame.
	std::mutex inputMutex;
	std::condition_variable inputReady;
	ofPixels input;
	bool hasInput = false;

	// Output hand-off.
	std::mutex outputMutex;
	Results output;
	bool hasNewOutput = false;
};

} // namespace ofxMediaPipe
