#pragma once

#include "ofMain.h"
#include "mediapipe/tasks/cc/vision/pose_landmarker/pose_landmarker.h"

namespace ofxMediaPipe {

enum class RunningMode { IMAGE, VIDEO };

struct PoseLandmark {
    glm::vec3 position;  // x, y normalized [0,1], z relative
    float visibility;
    float presence;
};

struct PoseLandmarkerResult {
    std::vector<std::vector<PoseLandmark>> multi_pose_landmarks;  // [pose][landmark]
    // Note: We are not outputting world landmarks or segmentation masks for simplicity.
};

class PoseLandmarker {
public:
    struct Options {
        std::string model_path = "pose_landmarker.task";
        RunningMode running_mode = RunningMode::VIDEO; // Default to video mode for webcam
        int num_poses = 1;
        float min_pose_detection_confidence = 0.5f;
        float min_pose_presence_confidence = 0.5f;
        float min_tracking_confidence = 0.5f;
        bool output_segmentation_masks = false;
    };
    
    bool setup(const Options& options);
    PoseLandmarkerResult detect(const ofPixels& pixels);  // IMAGE mode
    PoseLandmarkerResult detectForVideo(const ofPixels& pixels, int64_t timestamp_ms);  // VIDEO mode
    void close();
    bool isInitialized() const;
    
private:
    std::unique_ptr<mediapipe::tasks::vision::pose_landmarker::PoseLandmarker> landmarker_;
    Options options_;
    
    // Helper to convert ofPixels to mediapipe::Image
    mediapipe::Image convertToMpImage(const ofPixels& pixels);
};

} // namespace ofxMediaPipe