#include "ofApp.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

/// The eight canned gestures the model ships with, each given a colour.
/// "None" means a hand was found but matched no known gesture.
ofColor gestureColor(const std::string & name) {
	if (name == "Open_Palm") return ofColor(120, 230, 120);
	if (name == "Closed_Fist") return ofColor(240, 120, 120);
	if (name == "Pointing_Up") return ofColor(250, 210, 100);
	if (name == "Victory") return ofColor(140, 190, 255);
	if (name == "Thumb_Up") return ofColor(160, 255, 200);
	if (name == "Thumb_Down") return ofColor(255, 160, 120);
	if (name == "ILoveYou") return ofColor(230, 150, 250);
	return ofColor(170, 170, 170);
}

/// "Closed_Fist" -> "Closed Fist"
std::string prettify(std::string name) {
	std::replace(name.begin(), name.end(), '_', ' ');
	return name;
}

} // namespace

void ofApp::setup() {
	ofSetWindowTitle("ofxMediaPipe - gesture");
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	font.load(OF_TTF_SANS, 11, true, true);
	fontLarge.load(OF_TTF_SANS, 22, true, true);

	setupVideo();

	ofxMediaPipe::Tracker::Settings settings;
	// Pose is left off, so the worker neither loads that model nor spends any
	// time on it per pass -- a pass costs gesture time alone.
	settings.enablePose = false;
	settings.enableGesture = true;
	settings.gesture.modelPath = "gesture_recognizer.task";
	settings.gesture.numHands = 2;
	// Gestures scoring below this are reported as "None" rather than guessed at.
	settings.gesture.minGestureScore = 0.3f;
	settings.inferenceWidth = 256;
	tracker.setup(settings);
}

void ofApp::setupVideo() {
	// A Pi usually has no /dev/video0 -- its CSI camera lives behind libcamera,
	// and its other /dev/video* nodes are codec and ISP devices.
	// See apps/myApps/MediaPipeExample for CSI, movie and synthetic sources.
	if (ofFile::doesFileExist("/dev/video0")) {
		grabber.setDesiredFrameRate(30);
		grabber.setUseTexture(false);
		usingGrabber = grabber.setup(640, 480, false);
	}
	if (usingGrabber) {
		return;
	}

	ofDirectory dir(ofToDataPath("", true));
	for (const auto & extension : { "png", "jpg", "jpeg", "bmp" }) {
		dir.allowExt(extension);
	}
	dir.listDir();
	dir.sort();
	if (dir.size() > 0 && ofLoadImage(frame, dir.getPath(0))) {
		ofLogNotice("ofApp") << "no webcam, using still image " << dir.getName(0);
		if (frame.getPixelFormat() != OF_PIXELS_RGB
			&& frame.getPixelFormat() != OF_PIXELS_RGBA) {
			frame.setImageType(OF_IMAGE_COLOR);
		}
		texture.allocate(frame);
		texture.loadData(frame);
	} else {
		ofLogError("ofApp") << "no webcam and no image in bin/data";
	}
}

void ofApp::update() {
	if (usingGrabber) {
		grabber.update();
		if (grabber.isFrameNew()) {
			frame = grabber.getPixels();
			// Fed unmirrored even though the display is mirrored: flipping the
			// image would swap MediaPipe's Left/Right handedness labels.
			tracker.setPixels(frame);
			if (!texture.isAllocated() || texture.getWidth() != frame.getWidth()) {
				texture.allocate(frame);
			}
			texture.loadData(frame);
		}
	} else if (frame.isAllocated()) {
		// A still is fed until the model has been ready for a few seconds.
		// The task runs in VIDEO mode, which refines the answer across frames,
		// so one cold-start pass understates it -- but feeding an unchanging
		// image forever would pin a core recomputing the same result. Timing off
		// isReady() rather than app start avoids racing the model load, which
		// takes a few seconds on its own.
		if (tracker.isReady() && stillSettleDeadline == 0.f) {
			stillSettleDeadline = ofGetElapsedTimef() + 5.f;
		}
		if (stillSettleDeadline == 0.f || ofGetElapsedTimef() < stillSettleDeadline) {
			tracker.setPixels(frame);
		}
	}

	tracker.getResults(results);

	// React to what was recognized. Easing keeps a one-frame misclassification
	// from strobing the window.
	const std::string dominant = getDominantGesture();
	const ofFloatColor target = dominant.empty()
		? ofFloatColor(0.07f, 0.07f, 0.07f)
		: ofFloatColor(gestureColor(dominant)) * 0.35f;
	reactionColor = reactionColor.getLerped(target, 0.08f);
	ofBackground(ofColor(reactionColor));
}

std::string ofApp::getDominantGesture() const {
	std::string best;
	float bestScore = 0.f;
	for (const auto & hand : results.hands) {
		// "None" means no gesture matched, so it is not a winner.
		if (hand.gesture.categoryName.empty() || hand.gesture.categoryName == "None") {
			continue;
		}
		if (hand.gesture.score > bestScore) {
			bestScore = hand.gesture.score;
			best = hand.gesture.categoryName;
		}
	}
	return best;
}

ofRectangle ofApp::getVideoBounds() const {
	if (!texture.isAllocated()) {
		return ofRectangle(0, 0, ofGetWidth(), ofGetHeight());
	}
	ofRectangle video(0, 0, texture.getWidth(), texture.getHeight());
	video.scaleTo(ofRectangle(0, 0, ofGetWidth(), ofGetHeight()), OF_ASPECT_RATIO_KEEP);
	return video;
}

void ofApp::draw() {
	const ofRectangle bounds = getVideoBounds();

	ofPushMatrix();
	if (mirror) {
		ofTranslate(bounds.getCenter().x, 0);
		ofScale(-1, 1);
		ofTranslate(-bounds.getCenter().x, 0);
	}

	if (texture.isAllocated()) {
		ofSetColor(255);
		texture.draw(bounds);
	}

	// Each hand is tinted by its own gesture, so two hands can differ.
	for (const auto & hand : results.hands) {
		ofxMediaPipe::DrawStyle handStyle;
		handStyle.boneColor = gestureColor(hand.gesture.categoryName);
		handStyle.jointColor = ofColor(255, 255, 255, 230);
		handStyle.boneWidth = 2.f;
		handStyle.jointRadius = 3.f;
		ofxMediaPipe::drawHand(hand, bounds, handStyle);
	}
	ofPopMatrix();

	for (const auto & hand : results.hands) {
		drawGestureLabel(hand, bounds);
	}
	drawHud();
}

void ofApp::drawGestureLabel(const ofxMediaPipe::Hand & hand, const ofRectangle & bounds) {
	if (hand.landmarks.empty()) {
		return;
	}

	glm::vec2 anchor = ofxMediaPipe::toScreen(
		hand.landmarks[ofxMediaPipe::HandLandmarkIndex::Wrist], bounds);
	if (mirror) {
		anchor.x = bounds.getCenter().x * 2.f - anchor.x;
	}

	std::string text = hand.gesture.empty()
		? "hand"
		: prettify(hand.gesture.categoryName)
			+ "  " + ofToString(hand.gesture.score * 100.f, 0) + "%";
	// Handedness is reported from the person's point of view, not the camera's.
	if (!hand.handedness.empty()) {
		text = hand.handedness.categoryName + ": " + text;
	}

	const ofRectangle box = font.getStringBoundingBox(text, 0, 0);
	const float x = ofClamp(anchor.x - box.width * 0.5f, bounds.x + 4.f,
		bounds.getMaxX() - box.width - 16.f);
	const float y = anchor.y + 30.f;

	ofPushStyle();
	ofSetColor(0, 0, 0, 180);
	ofDrawRectRounded(x - 6.f, y - box.height - 6.f, box.width + 12.f, box.height + 12.f, 4.f);
	ofSetColor(gestureColor(hand.gesture.categoryName));
	font.drawString(text, x, y);
	ofPopStyle();
}

void ofApp::drawHud() {
	std::vector<std::string> lines {
		std::string("source:  ") + (usingGrabber ? "webcam" : "still image in bin/data"),
	};
	if (tracker.isFailed()) {
		lines.push_back("model:   FAILED - " + tracker.getError());
	} else if (!tracker.isReady()) {
		lines.push_back("model:   loading...");
	} else {
		lines.push_back("gesture: " + ofToString(results.hands.size()) + " hand(s), "
			+ ofToString(results.gestureMs, 0) + " ms");
		lines.push_back("rate:    " + ofToString(tracker.getInferenceFps(), 1)
			+ " passes/sec");
	}
	lines.push_back("display: " + ofToString(ofGetFrameRate(), 0) + " fps");
	lines.push_back("");
	lines.push_back("[m] mirror   [f] fullscreen");

	float width = 0.f;
	for (const auto & line : lines) {
		width = std::max(width, font.getStringBoundingBox(line, 0, 0).width);
	}

	ofPushStyle();
	ofSetColor(0, 0, 0, 180);
	ofDrawRectRounded(10, 10, width + 24.f, lines.size() * 17.f + 20.f, 6.f);
	ofSetColor(235);
	float y = 34.f;
	for (const auto & line : lines) {
		font.drawString(line, 22, y);
		y += 17.f;
	}
	ofPopStyle();

	// The reaction, spelled out: whatever gesture currently wins also tints the
	// window background.
	const std::string dominant = getDominantGesture();
	if (!dominant.empty()) {
		const std::string text = prettify(dominant);
		const ofRectangle box = fontLarge.getStringBoundingBox(text, 0, 0);
		ofSetColor(gestureColor(dominant));
		fontLarge.drawString(text, ofGetWidth() - box.width - 24.f, ofGetHeight() - 24.f);
	}
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case 'm': mirror = !mirror; break;
	case 'f': ofToggleFullscreen(); break;
	default: break;
	}
}

void ofApp::exit() {
	tracker.stop();
}
