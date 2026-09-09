#include "ofxMediaPipeTracker.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ofxMediaPipe {

namespace {
uint64_t nowMs() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}

Tracker::~Tracker() {
	stop();
}

void Tracker::setup(const Settings & s) {
	stop();
	settings = s;
	ready = false;
	failed = false;
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		error.clear();
	}
	startThread();
}

void Tracker::stop() {
	// Not guarded by isThreadRunning(): a worker that failed to load its models
	// has already cleared that flag on its way out, and its thread still needs
	// joining. stopThread() and waitForThread() are both no-ops when the thread
	// was never started.
	stopThread();
	// Wake the worker so it observes the stop request rather than blocking on
	// the input condition variable until the next frame arrives.
	inputReady.notify_all();
	waitForThread(false);

	pose.close();
	gesture.close();
	ready = false;
}

std::string Tracker::getError() const {
	std::lock_guard<std::mutex> lock(errorMutex);
	return error;
}

void Tracker::setPixels(const ofPixels & pixels) {
	if (!pixels.isAllocated()) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(inputMutex);
		// Overwrite whatever is pending: only the newest frame is worth running.
		input = pixels;
		hasInput = true;
	}
	inputReady.notify_one();
}

bool Tracker::getResults(Results & out) {
	std::lock_guard<std::mutex> lock(outputMutex);
	if (!hasNewOutput) {
		return false;
	}
	out = output;
	hasNewOutput = false;
	return true;
}

void Tracker::threadedFunction() {
	// Load the models here so that every MediaPipe call for a given task
	// happens on one thread for the task's whole lifetime.
	bool ok = true;
	if (settings.enablePose) {
		ok = pose.setup(settings.pose);
		if (!ok) {
			std::lock_guard<std::mutex> lock(errorMutex);
			error = pose.getError();
		}
	}
	if (ok && settings.enableGesture) {
		ok = gesture.setup(settings.gesture);
		if (!ok) {
			std::lock_guard<std::mutex> lock(errorMutex);
			error = gesture.getError();
		}
	}
	if (!ok) {
		failed = true;
		return;
	}
	ready = true;

	// MediaPipe's VIDEO mode requires strictly increasing timestamps, so the
	// stream clock is kept here rather than trusting frame arrival times.
	uint64_t timestampMs = 0;
	uint64_t lastFpsSample = nowMs();
	int passes = 0;

	ofPixels frame;
	while (isThreadRunning()) {
		{
			std::unique_lock<std::mutex> lock(inputMutex);
			inputReady.wait(lock, [this] { return hasInput || !isThreadRunning(); });
			if (!isThreadRunning()) {
				break;
			}
			frame = input;
			hasInput = false;
		}

		if (settings.inferenceWidth > 0 && frame.getWidth() > (size_t)settings.inferenceWidth) {
			const float scale = settings.inferenceWidth / (float)frame.getWidth();
			frame.resize(settings.inferenceWidth, std::max(1, (int)std::round(frame.getHeight() * scale)));
		}

		Results results;
		results.timestampMs = ++timestampMs;

		if (settings.enablePose) {
			const uint64_t start = nowMs();
			if (!pose.detect(frame, timestampMs, results.poses)) {
				ofLogWarning("ofxMediaPipe::Tracker") << "pose: " << pose.getError();
			}
			results.poseMs = (float)(nowMs() - start);
		}
		if (settings.enableGesture) {
			const uint64_t start = nowMs();
			if (!gesture.recognize(frame, timestampMs, results.hands)) {
				ofLogWarning("ofxMediaPipe::Tracker") << "gesture: " << gesture.getError();
			}
			results.gestureMs = (float)(nowMs() - start);
		}

		{
			std::lock_guard<std::mutex> lock(outputMutex);
			output = std::move(results);
			hasNewOutput = true;
		}

		++passes;
		const uint64_t elapsed = nowMs() - lastFpsSample;
		if (elapsed >= 1000) {
			inferenceFps = passes * 1000.f / (float)elapsed;
			passes = 0;
			lastFpsSample = nowMs();
		}
	}

	pose.close();
	gesture.close();
}

} // namespace ofxMediaPipe
