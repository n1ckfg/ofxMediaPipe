#pragma once

#include "ofMain.h"
#include "ofxMediaPipe.h"

/// Pose tracking and gesture recognition together on one feed.
///
/// The body is drawn as a 33-point skeleton; each hand is drawn as a 21-point
/// skeleton tinted by whichever gesture the model recognized, and the app
/// reacts to that gesture rather than only printing it.
///
/// Both models share one worker thread inside ofxMediaPipe::Tracker, so a pass
/// costs pose time plus gesture time. That is the price of running both.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;
	void keyPressed(int key) override;

private:
	void setupVideo();
	ofRectangle getVideoBounds() const;
	void drawGestureLabel(const ofxMediaPipe::Hand & hand, const ofRectangle & bounds);
	void drawHud();

	/// The highest-scoring gesture across every detected hand, or "" for none.
	std::string getDominantGesture() const;

	ofVideoGrabber grabber;
	bool usingGrabber = false;
	ofPixels frame;
	ofTexture texture;

	ofxMediaPipe::Tracker tracker;
	ofxMediaPipe::Tracker::Results results;

	/// Eased towards the dominant gesture's colour, so the reaction is not a
	/// hard flicker when the classifier changes its mind between frames.
	ofFloatColor reactionColor { 0.07f, 0.07f, 0.07f };

	/// Set once the models are ready; the still is fed until it passes, so
	/// VIDEO-mode tracking has time to settle. 0 means "not ready yet".
	float stillSettleDeadline = 0.f;

	ofTrueTypeFont font;
	ofTrueTypeFont fontLarge;
	bool mirror = true;
	bool showPose = true;
};
