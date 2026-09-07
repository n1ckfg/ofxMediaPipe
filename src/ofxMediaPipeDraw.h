#pragma once

#include "ofMain.h"
#include "ofxMediaPipeTypes.h"

namespace ofxMediaPipe {

struct DrawStyle {
	ofColor boneColor = ofColor(0, 220, 190);
	ofColor jointColor = ofColor(255, 255, 255);
	float boneWidth = 2.f;
	float jointRadius = 3.f;
	/// Landmarks below this visibility are skipped, along with their bones.
	/// Ignored for models that do not report visibility (such as hands).
	float minVisibility = 0.5f;
};

/// Maps a normalized landmark into `bounds`, which is where the frame the
/// landmarks came from is drawn on screen.
glm::vec2 toScreen(const Landmark & landmark, const ofRectangle & bounds);

void drawPose(const Pose & pose, const ofRectangle & bounds, const DrawStyle & style = DrawStyle());
void drawHand(const Hand & hand, const ofRectangle & bounds, const DrawStyle & style = DrawStyle());

} // namespace ofxMediaPipe
