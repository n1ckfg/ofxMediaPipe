#pragma once

#include "ofMain.h"
#include "ofxMediaPipe.h"

/// Basic pose tracking: one body, 33 landmarks, drawn over the camera feed.
///
/// Inference runs on ofxMediaPipe::Tracker's worker thread. A pose pass costs
/// well over a frame's budget on a Raspberry Pi, so doing it inline in update()
/// would drag the whole app down to the model's rate.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;
	void keyPressed(int key) override;

private:
	/// Opens a webcam, or falls back to a still image in bin/data.
	void setupVideo();
	/// Where the frame is drawn, letterboxed into the window.
	ofRectangle getVideoBounds() const;
	void drawLandmarkLabel(const ofxMediaPipe::Pose & pose, int index,
		const ofRectangle & bounds);

	ofVideoGrabber grabber;
	bool usingGrabber = false;
	ofPixels frame;
	ofTexture texture;

	ofxMediaPipe::Tracker tracker;
	ofxMediaPipe::Tracker::Results results;

	/// Set once the models are ready; the still is fed until it passes, so
	/// VIDEO-mode tracking has time to settle. 0 means "not ready yet".
	float stillSettleDeadline = 0.f;

	ofTrueTypeFont font;
	bool mirror = true;
	bool showLabels = true;
};
