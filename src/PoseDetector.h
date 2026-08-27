#pragma once

#include "ofMain.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/util/color.pb.h"
#include "mediapipe/util/render_data.pb.h"
#include "mediapipe/util/tracking.pb.h"

namespace ofxMediaPipe {

struct PoseLandmark {
    float x, y, z;  // Normalized coordinates [0, 1] for x, y; relative depth for z
    float visibility;
    float presence;
};

struct PoseLandmarkerResult {
    std::vector<std::vector<PoseLandmark>> multi_pose_landmarks;  // [pose][landmark]
};

class PoseDetector {
public:
    struct Options {
        std::string graph_path = "mediapipe/graphs/pose_tracking/pose_tracking_cpu.pbtxt";
        bool run_on_gpu = false;
    };
    
    bool setup(const Options& options);
    void detect(ofPixels& pixels, PoseLandmarkerResult& result);
    void close();
    bool isInitialized() const;
    
private:
    mediapipe::CalculatorGraph graph_;
    bool initialized_ = false;
    std::string input_stream_ = "input_video";
    std::string output_stream_ = "pose_landmarks";
    bool run_on_gpu_ = false;
    mediapipe::OutputStreamPoller poller_;
    
    // Helper to convert ofPixels to mediapipe::ImageFrame
    std::unique_ptr<mediapipe::ImageFrame> convertToImageFrame(const ofPixels& pixels);
};

} // namespace ofxMediaPipe