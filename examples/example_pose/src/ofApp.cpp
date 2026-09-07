#include "ofApp.h"

#include <algorithm>
#include <string>
#include <vector>

void ofApp::setup() {
	ofSetWindowTitle("ofxMediaPipe - pose");
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	ofBackground(18);
	font.load(OF_TTF_SANS, 11, true, true);

	setupVideo();

	// Only the pose model is loaded; leaving the gesture model off keeps the
	// worker free to run pose as often as possible.
	ofxMediaPipe::Tracker::Settings settings;
	settings.enablePose = true;
	settings.enableGesture = false;
	settings.pose.modelPath = "pose_landmarker_lite.task";
	settings.pose.numPoses = 1;
	// Landmarks come back normalized, so inferring on a smaller frame still
	// lines up with the full-size image when drawn.
	settings.inferenceWidth = 256;
	tracker.setup(settings);
}

void ofApp::setupVideo() {
	// A Pi usually has no /dev/video0 -- its CSI camera lives behind libcamera,
	// and its other /dev/video* nodes are codec and ISP devices. Checking first
	// avoids ofVideoGrabber logging its way through devices it cannot open.
	// See apps/myApps/MediaPipeExample for CSI, movie and synthetic sources.
	if (ofFile::doesFileExist("/dev/video0")) {
		grabber.setDesiredFrameRate(30);
		grabber.setUseTexture(false);
		usingGrabber = grabber.setup(640, 480, false);
	}
	if (usingGrabber) {
		return;
	}

	// No webcam: run on the first image in bin/data instead, so the example is
	// still demonstrable. Drop a photo of a person in there.
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

			// Hand the newest frame to the worker. Frames arriving while it is
			// busy replace the pending one rather than queueing, so results stay
			// in step with the camera instead of falling behind.
			tracker.setPixels(frame);

			if (!texture.isAllocated() || texture.getWidth() != frame.getWidth()) {
				texture.allocate(frame);
			}
			texture.loadData(frame);
		}
	} else if (frame.isAllocated()) {
		// A still is fed until the models have been ready for a few seconds.
		// The model runs in VIDEO mode, which refines its answer across frames,
		// so one cold-start pass understates it -- but feeding an unchanging
		// image forever would pin a core recomputing the same result.
		if (tracker.isReady() && stillSettleDeadline == 0.f) {
			stillSettleDeadline = ofGetElapsedTimef() + 5.f;
		}
		if (stillSettleDeadline == 0.f || ofGetElapsedTimef() < stillSettleDeadline) {
			tracker.setPixels(frame);
		}
	}

	// Returns false when no new pass has finished; `results` then keeps the
	// previous one, which is what we want to keep drawing.
	tracker.getResults(results);
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
		// Flip about the frame's own centre so the overlay lands on the image.
		ofTranslate(bounds.getCenter().x, 0);
		ofScale(-1, 1);
		ofTranslate(-bounds.getCenter().x, 0);
	}

	if (texture.isAllocated()) {
		ofSetColor(255);
		texture.draw(bounds);
	}

	ofxMediaPipe::DrawStyle style;
	style.boneColor = ofColor(0, 220, 190);
	style.jointColor = ofColor(255, 255, 255, 220);
	style.boneWidth = 3.f;
	style.jointRadius = 4.f;
	// Landmarks the model believes are occluded or off-frame are skipped.
	style.minVisibility = 0.5f;

	for (const auto & pose : results.poses) {
		ofxMediaPipe::drawPose(pose, bounds, style);
	}
	ofPopMatrix();

	// Labels sit outside the mirror transform so the text is never reversed.
	if (showLabels) {
		using namespace ofxMediaPipe::PoseLandmarkIndex;
		for (const auto & pose : results.poses) {
			for (const int index : { Nose, LeftWrist, RightWrist }) {
				drawLandmarkLabel(pose, index, bounds);
			}
		}
	}

	std::vector<std::string> lines {
		std::string("source:  ") + (usingGrabber ? "webcam" : "still image in bin/data"),
		tracker.isFailed()
			? "pose:    FAILED - " + tracker.getError()
			: (tracker.isReady()
				? "pose:    " + ofToString(results.poses.size()) + " detected, "
					+ ofToString(results.poseMs, 0) + " ms/frame"
				: std::string("pose:    loading model...")),
		"display: " + ofToString(ofGetFrameRate(), 0) + " fps",
		"",
		"[m] mirror   [l] labels   [f] fullscreen",
	};

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
}

void ofApp::drawLandmarkLabel(const ofxMediaPipe::Pose & pose, int index,
	const ofRectangle & bounds) {

	if (index >= (int)pose.landmarks.size()) {
		return;
	}
	const ofxMediaPipe::Landmark & landmark = pose.landmarks[index];
	if (landmark.hasVisibility && landmark.visibility < 0.5f) {
		return;
	}

	glm::vec2 point = ofxMediaPipe::toScreen(landmark, bounds);
	if (mirror) {
		point.x = bounds.getCenter().x * 2.f - point.x;
	}

	const std::string & name = ofxMediaPipe::getPoseLandmarkNames()[index];

	ofPushStyle();
	ofSetColor(255, 220, 90);
	ofNoFill();
	ofSetLineWidth(2.f);
	ofDrawCircle(point, 9.f);
	ofFill();
	font.drawString(name, point.x + 13.f, point.y + 4.f);
	ofPopStyle();
}

void ofApp::keyPressed(int key) {
	switch (key) {
	case 'm': mirror = !mirror; break;
	case 'l': showLabels = !showLabels; break;
	case 'f': ofToggleFullscreen(); break;
	default: break;
	}
}

void ofApp::exit() {
	// Stop the worker before the models and the camera go away.
	tracker.stop();
}
