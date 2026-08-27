#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSetVerticalSync(true);
    ofSetFrameRate(30);
    
    // Initialize webcam
    cam.setup(640, 480);
    
    // Setup pose landmarker
    ofxMediaPipe::PoseLandmarker::Options options;
    options.model_path = "pose_landmarker.task"; // Will be looked up in data folder via ofToDataPath
    options.running_mode = ofxMediaPipe::RunningMode::VIDEO;
    options.num_poses = 1;
    options.min_pose_detection_confidence = 0.5f;
    options.min_pose_presence_confidence = 0.5f;
    options.min_tracking_confidence = 0.5f;
    options.output_segmentation_masks = false;
    
    bSetupSuccess = poseLandmarker.setup(options);
    if (!bSetupSuccess) {
        ofLogError("ofApp") << "Failed to setup pose landmarker. Check that the model file exists and the MediaPipe library is built.";
    }
    
    // Initialize timestamp for video mode
    lastTimestampMs = 0;
    
    // Load font for debugging
    font.load("fonts/DroidSans.ttf", 14);
}

//--------------------------------------------------------------
void ofApp::update(){
    cam.update();
    if (cam.isFrameNew()) {
        // Get the current frame as an ofPixels
        ofPixels pixels = cam.getPixels();
        
        // Increment timestamp for each frame (in milliseconds)
        // We use the system time in milliseconds for simplicity, but note that it must be monotonically increasing.
        // Alternatively, we could use a frame counter and assume a fixed frame rate.
        auto now = std::chrono::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        // Ensure timestamp is increasing (if we go backwards, we clamp to lastTimestampMs+1)
        if (timestamp_ms <= lastTimestampMs) {
            timestamp_ms = lastTimestampMs + 1;
        }
        lastTimestampMs = timestamp_ms;
        
        // Run pose landmarker detection on the video frame
        latestResult = poseLandmarker.detectForVideo(pixels, timestamp_ms);
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    ofBackground(0);
    
    if (cam.isInitialized()) {
        cam.draw(0, 0);
    }
    
    // Draw pose landmarks if available
    if (bSetupSuccess && !latestResult.multi_pose_landmarks.empty()) {
        ofPushStyle();
        ofSetLineWidth(2);
        for (const auto& pose : latestResult.multi_pose_landmarks) {
            for (const auto& landmark : pose) {
                // Convert normalized coordinates to screen space
                float x = landmark.position.x * cam.getWidth();
                float y = landmark.position.y * cam.getHeight();
                // Draw a circle for each landmark
                ofDrawCircle(x, y, 4);
                // Optionally draw visibility as alpha or size
                ofSetColor(255, 255, 255, landmark.visibility * 255);
                ofDrawCircle(x, y, 2);
                ofSetColor(255);
            }
        }
        ofPopStyle();
    }
    
    // Draw status
    ofSetColor(255);
    string status = "Pose Landmarker: " + string(bSetupSuccess ? "READY" : "FAILED");
    if (!bSetupSuccess) {
        status += " - Check logs for details.";
    }
    font.drawString(status, 20, 20);
    
    if (bSetupSuccess) {
        string mode = "VIDEO";
        font.drawString("Mode: " + mode, 20, 40);
        font.drawString("Num poses: " + ofToString(latestResult.multi_pose_landmarks.size()), 20, 60);
        font.drawString("Timestamp: " + ofToString(lastTimestampMs), 20, 80);
    }
}

//--------------------------------------------------------------
void ofApp::exit(){
    cam.close();
    poseLandmarker.close();
}