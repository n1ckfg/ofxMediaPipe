#include "PoseLandmarker.h"
#include "ofUtils.h"
#include "ofLog.h"
#include "mediapipe/tasks/cc/vision/core/image_utils.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h"

namespace ofxMediaPipe {

mediapipe::Image PoseLandmarker::convertToMpImage(const ofPixels& pixels) {
    // Ensure we have RGB data
    if (pixels.getNumChannels() != 3) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Input pixels must have 3 channels (RGB).";
        return mediapipe::Image();
    }

    // The mediapipe::Image constructor that takes external data does not take ownership of the data.
    // We must ensure the data lives long enough. We will copy the data into an ImageFrame and then create an Image from that.
    // Alternatively, we can use the ImageFrameWrapper to wrap the data, but we must ensure the data is not deleted.
    // Since we are passing the pixels by reference and the pixels are valid until the next frame, we must copy if we are going to use it asynchronously.
    // For simplicity in this example, we assume the detectForVideo is called immediately and the pixels are not changed until the next call.
    // We will create an ImageFrame with the pixel data and then create an Image from that ImageFrame.

    // Create an ImageFrame with the pixel data
    auto image_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, pixels.getWidth(), pixels.getHeight(),
        mediapipe::ImageFrame::kDefaultAlignmentBoundary);
    // Copy the pixel data
    memcpy(image_frame->MutablePixelData(), pixels.getData(),
           pixels.getWidth() * pixels.getHeight() * pixels.getNumChannels() * sizeof(uint8_t));

    // Create an Image from the ImageFrame
    mediapipe::Image mp_image = mediapipe::Image(std::move(image_frame));
    return mp_image;
}

bool PoseLandmarker::setup(const Options& options) {
    close(); // Close any existing landmarker

    this->options_ = options;

    // Check if the model file exists
    std::string model_path = ofToDataPath(options.model_path, true);
    if (!ofFile::doesFileExist(model_path)) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Model file not found: " << model_path;
        return false;
    }

    // Set up base options
    mediapipe::tasks::core::BaseOptions base_options;
    base_options.model_asset_path = model_path;
    // TODO: Set up delegate (e.g., GPU) if needed
    base_options.delegate = mediapipe::tasks::core::BaseOptions::Delegate::CPU;

    // Set up pose landmarker options
    mediapipe::tasks::vision::pose_landmarker::PoseLandmarkerOptions pose_options;
    pose_options.base_options = base_options;
    pose_options.running_mode = static_cast<mediapipe::tasks::vision::core::RunningMode>(options.running_mode);
    pose_options.num_poses = options.num_poses;
    pose_options.min_pose_detection_confidence = options.min_pose_detection_confidence;
    pose_options.min_pose_presence_confidence = options.min_pose_presence_confidence;
    pose_options.min_tracking_confidence = options.min_tracking_confidence;
    pose_options.output_segmentation_masks = options.output_segmentation_masks;
    // Note: We are not outputting blendshapes or transformation matrices in this example.

    try {
        landmarker_ = mediapipe::tasks::vision::pose_landmarker::PoseLandmarker::CreateFromOptions(pose_options);
    } catch (const std::exception& e) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Failed to create pose landmarker: " << e.what();
        return false;
    }

    if (!landmarker_) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Failed to create pose landmarker (null pointer).";
        return false;
    }

    return true;
}

void PoseLandmarker::close() {
    landmarker_.reset();
}

bool PoseLandmarker::isInitialized() const {
    return landmarker_ != nullptr;
}

PoseLandmarkerResult PoseLandmarker::detect(const ofPixels& pixels) {
    if (!isInitialized()) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Pose landmarker not initialized.";
        return PoseLandmarkerResult();
    }

    // Convert the pixels to a MediaPipe Image
    mediapipe::Image mp_image = convertToMpImage(pixels);
    if (!mp_image.is_valid()) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Failed to convert pixels to MediaPipe Image.";
        return PoseLandmarkerResult();
    }

    // Run pose landmarker detection on the image
    mediapipe::tasks::vision::pose_landmarker::PoseLandmarkerResult result;
    MP_RETURN_IF_ERROR(landmarker_->Detect(mp_image, &result));

    // Convert the result to our format
    PoseLandmarkerResult our_result;
    our_result.multi_pose_landmarks.resize(result.pose_landmarks.size());
    for (size_t i = 0; i < result.pose_landmarks.size(); ++i) {
        const auto& landmarks = result.pose_landmarks[i];
        our_result.multi_pose_landmarks[i].resize(landmarks.size());
        for (size_t j = 0; j < landmarks.size(); ++j) {
            our_result.multi_pose_landmarks[i][j].position =
                glm::vec3(landmarks[j].x, landmarks[j].y, landmarks[j].z);
            our_result.multi_pose_landmarks[i][j].visibility = landmarks[j].visibility;
            our_result.multi_pose_landmarks[i][j].presence = landmarks[j].presence;
        }
    }

    return our_result;
}

PoseLandmarkerResult PoseLandmarker::detectForVideo(const ofPixels& pixels, int64_t timestamp_ms) {
    if (!isInitialized()) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Pose landmarker not initialized.";
        return PoseLandmarkerResult();
    }

    // Convert the pixels to a MediaPipe Image
    mediapipe::Image mp_image = convertToMpImage(pixels);
    if (!mp_image.is_valid()) {
        ofLogError("ofxMediaPipe::PoseLandmarker") << "Failed to convert pixels to MediaPipe Image.";
        return PoseLandmarkerResult();
    }

    // Run pose landmarker detection on the video frame
    mediapipe::tasks::vision::pose_landmarker::PoseLandmarkerResult result;
    MP_RETURN_IF_ERROR(landmarker_->DetectForVideo(mp_image, timestamp_ms, &result));

    // Convert the result to our format
    PoseLandmarkerResult our_result;
    our_result.multi_pose_landmarks.resize(result.pose_landmarks.size());
    for (size_t i = 0; i < result.pose_landmarks.size(); ++i) {
        const auto& landmarks = result.pose_landmarks[i];
        our_result.multi_pose_landmarks[i].resize(landmarks.size());
        for (size_t j = 0; j < landmarks.size(); ++j) {
            our_result.multi_pose_landmarks[i][j].position =
                glm::vec3(landmarks[j].x, landmarks[j].y, landmarks[j].z);
            our_result.multi_pose_landmarks[i][j].visibility = landmarks[j].visibility;
            our_result.multi_pose_landmarks[i][j].presence = landmarks[j].presence;
        }
    }

    return our_result;
}

} // namespace ofxMediaPipe