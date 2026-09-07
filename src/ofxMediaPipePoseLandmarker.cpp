#include "ofxMediaPipePoseLandmarker.h"

#include "ofxMediaPipeInternal.h"

#include "mediapipe/tasks/c/vision/pose_landmarker/pose_landmarker.h"

namespace ofxMediaPipe {

struct PoseLandmarker::Impl {
	MpPoseLandmarkerPtr landmarker = nullptr;
	Settings settings;
	std::string error;
	// Reused between frames so a format conversion does not reallocate.
	ofPixels scratch;
};

PoseLandmarker::PoseLandmarker()
	: impl(std::make_unique<Impl>()) { }

PoseLandmarker::~PoseLandmarker() {
	close();
}

bool PoseLandmarker::isSetup() const {
	return impl->landmarker != nullptr;
}

const std::string & PoseLandmarker::getError() const {
	return impl->error;
}

bool PoseLandmarker::setup() {
	return setup(Settings());
}

bool PoseLandmarker::setup(const Settings & settings) {
	close();
	impl->settings = settings;
	impl->error.clear();

	const std::string modelPath = ofFilePath::isAbsolute(settings.modelPath)
		? settings.modelPath
		: ofToDataPath(settings.modelPath, true);

	if (!ofFile::doesFileExist(modelPath)) {
		impl->error = "pose model not found: " + modelPath;
		ofLogError("ofxMediaPipe::PoseLandmarker") << impl->error;
		return false;
	}

	PoseLandmarkerOptions options {};
	options.base_options.model_asset_path = modelPath.c_str();
	options.base_options.delegate = CPU;
	options.running_mode = VIDEO;
	options.num_poses = settings.numPoses;
	options.min_pose_detection_confidence = settings.minDetectionConfidence;
	options.min_pose_presence_confidence = settings.minPresenceConfidence;
	options.min_tracking_confidence = settings.minTrackingConfidence;
	options.output_segmentation_masks = false;
	options.result_callback = nullptr;

	char * error = nullptr;
	const MpStatus status = MpPoseLandmarkerCreate(&options, &impl->landmarker, &error);
	if (status != kMpOk || impl->landmarker == nullptr) {
		impl->landmarker = nullptr;
		impl->error = internal::takeError(error, "MpPoseLandmarkerCreate failed");
		ofLogError("ofxMediaPipe::PoseLandmarker") << impl->error;
		return false;
	}

	ofLogNotice("ofxMediaPipe::PoseLandmarker")
		<< "ready (" << settings.numPoses << " pose(s), model " << modelPath << ")";
	return true;
}

void PoseLandmarker::close() {
	if (impl->landmarker == nullptr) {
		return;
	}
	char * error = nullptr;
	if (MpPoseLandmarkerClose(impl->landmarker, &error) != kMpOk) {
		ofLogWarning("ofxMediaPipe::PoseLandmarker")
			<< internal::takeError(error, "MpPoseLandmarkerClose failed");
	}
	impl->landmarker = nullptr;
}

bool PoseLandmarker::detect(const ofPixels & pixels, uint64_t timestampMs,
	std::vector<Pose> & outPoses) {

	if (!isSetup()) {
		impl->error = "detect() called before setup()";
		return false;
	}

	internal::ScopedImage image;
	if (!internal::makeImage(pixels, impl->scratch, image, impl->error)) {
		return false;
	}

	PoseLandmarkerResult result {};
	char * error = nullptr;
	const MpStatus status = MpPoseLandmarkerDetectForVideo(
		impl->landmarker, image.get(), nullptr,
		static_cast<int64_t>(timestampMs), &result, &error);

	if (status != kMpOk) {
		impl->error = internal::takeError(error, "MpPoseLandmarkerDetectForVideo failed");
		return false;
	}

	outPoses.clear();
	outPoses.reserve(result.pose_landmarks_count);
	for (uint32_t i = 0; i < result.pose_landmarks_count; ++i) {
		Pose pose;
		pose.landmarks = internal::toLandmarks(result.pose_landmarks[i]);
		// World landmarks are optional and are reported as a parallel array.
		if (result.pose_world_landmarks != nullptr && i < result.pose_world_landmarks_count) {
			pose.worldLandmarks = internal::toLandmarks(result.pose_world_landmarks[i]);
		}
		outPoses.push_back(std::move(pose));
	}

	MpPoseLandmarkerCloseResult(&result);
	impl->error.clear();
	return true;
}

} // namespace ofxMediaPipe
