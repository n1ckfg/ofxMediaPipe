#pragma once

#include "ofMain.h"
#include "PoseLandmarker.h"

class ofApp : public ofBaseApp {

public:
    void setup();
    void update();
    void draw();
    void exit();

    ofVideoGrabber cam;
    ofxMediaPipe::PoseLandmarker poseLandmarker;
    bool bSetupSuccess;
    ofxMediaPipe::PoseLandmarkerResult latestResult;
    ofTrueTypeFont font;
    uint64_t lastTimestampMs; // For video mode, we need to provide timestamps
};