#include "ofxMediaPipeGestureRecognizer.h"

#include "ofxMediaPipeInternal.h"

#include "mediapipe/tasks/c/vision/gesture_recognizer/gesture_recognizer.h"

namespace ofxMediaPipe {

struct GestureRecognizer::Impl {
	MpGestureRecognizerPtr recognizer = nullptr;
	Settings settings;
	std::string error;
	ofPixels scratch;
};

GestureRecognizer::GestureRecognizer()
	: impl(std::make_unique<Impl>()) { }

GestureRecognizer::~GestureRecognizer() {
	close();
}

bool GestureRecognizer::isSetup() const {
	return impl->recognizer != nullptr;
}

const std::string & GestureRecognizer::getError() const {
	return impl->error;
}

bool GestureRecognizer::setup() {
	return setup(Settings());
}

bool GestureRecognizer::setup(const Settings & settings) {
	close();
	impl->settings = settings;
	impl->error.clear();

	const std::string modelPath = ofFilePath::isAbsolute(settings.modelPath)
		? settings.modelPath
		: ofToDataPath(settings.modelPath, true);

	if (!ofFile::doesFileExist(modelPath)) {
		impl->error = "gesture model not found: " + modelPath;
		ofLogError("ofxMediaPipe::GestureRecognizer") << impl->error;
		return false;
	}

	GestureRecognizerOptions options {};
	options.base_options.model_asset_path = modelPath.c_str();
	options.base_options.delegate = CPU;
	options.running_mode = VIDEO;
	options.num_hands = settings.numHands;
	options.min_hand_detection_confidence = settings.minDetectionConfidence;
	options.min_hand_presence_confidence = settings.minPresenceConfidence;
	options.min_tracking_confidence = settings.minTrackingConfidence;
	options.result_callback = nullptr;

	// Both classifiers must be given a sane max_results: the C API treats 0 as
	// invalid, and the struct has no default for it.
	options.canned_gestures_classifier_options.max_results = 1;
	options.canned_gestures_classifier_options.score_threshold = settings.minGestureScore;
	options.custom_gestures_classifier_options.max_results = 1;
	options.custom_gestures_classifier_options.score_threshold = settings.minGestureScore;

	char * error = nullptr;
	const MpStatus status = MpGestureRecognizerCreate(&options, &impl->recognizer, &error);
	if (status != kMpOk || impl->recognizer == nullptr) {
		impl->recognizer = nullptr;
		impl->error = internal::takeError(error, "MpGestureRecognizerCreate failed");
		ofLogError("ofxMediaPipe::GestureRecognizer") << impl->error;
		return false;
	}

	ofLogNotice("ofxMediaPipe::GestureRecognizer")
		<< "ready (" << settings.numHands << " hand(s), model " << modelPath << ")";
	return true;
}

void GestureRecognizer::close() {
	if (impl->recognizer == nullptr) {
		return;
	}
	char * error = nullptr;
	if (MpGestureRecognizerClose(impl->recognizer, &error) != kMpOk) {
		ofLogWarning("ofxMediaPipe::GestureRecognizer")
			<< internal::takeError(error, "MpGestureRecognizerClose failed");
	}
	impl->recognizer = nullptr;
}

bool GestureRecognizer::recognize(const ofPixels & pixels, uint64_t timestampMs,
	std::vector<Hand> & outHands) {

	if (!isSetup()) {
		impl->error = "recognize() called before setup()";
		return false;
	}

	internal::ScopedImage image;
	if (!internal::makeImage(pixels, impl->scratch, image, impl->error)) {
		return false;
	}

	GestureRecognizerResult result {};
	char * error = nullptr;
	const MpStatus status = MpGestureRecognizerRecognizeForVideo(
		impl->recognizer, image.get(), nullptr,
		static_cast<int64_t>(timestampMs), &result, &error);

	if (status != kMpOk) {
		impl->error = internal::takeError(error, "MpGestureRecognizerRecognizeForVideo failed");
		return false;
	}

	// Every per-hand field is its own array; the hand count is the landmark
	// count, and the others are indexed in step with it.
	outHands.clear();
	outHands.reserve(result.hand_landmarks_count);
	for (uint32_t i = 0; i < result.hand_landmarks_count; ++i) {
		Hand hand;
		hand.landmarks = internal::toLandmarks(result.hand_landmarks[i]);
		if (result.hand_world_landmarks != nullptr && i < result.hand_world_landmarks_count) {
			hand.worldLandmarks = internal::toLandmarks(result.hand_world_landmarks[i]);
		}
		// Each Categories entry is already sorted best-first.
		if (result.gestures != nullptr && i < result.gestures_count
			&& result.gestures[i].categories_count > 0) {
			hand.gesture = internal::toCategory(result.gestures[i].categories[0]);
		}
		if (result.handedness != nullptr && i < result.handedness_count
			&& result.handedness[i].categories_count > 0) {
			hand.handedness = internal::toCategory(result.handedness[i].categories[0]);
		}
		outHands.push_back(std::move(hand));
	}

	MpGestureRecognizerCloseResult(&result);
	impl->error.clear();
	return true;
}

} // namespace ofxMediaPipe
