#include "ofxMediaPipeDraw.h"

#include <utility>

namespace ofxMediaPipe {

namespace {

bool isVisible(const Landmark & landmark, float minVisibility) {
	// Hand landmarks carry no visibility score, so treat "unreported" as visible.
	return !landmark.hasVisibility || landmark.visibility >= minVisibility;
}

void drawSkeleton(const std::vector<Landmark> & landmarks,
	const std::vector<std::pair<int, int>> & connections,
	const ofRectangle & bounds, const DrawStyle & style) {

	if (landmarks.empty()) {
		return;
	}

	ofPushStyle();
	ofSetLineWidth(style.boneWidth);
	ofSetColor(style.boneColor);
	for (const auto & connection : connections) {
		const int a = connection.first;
		const int b = connection.second;
		if (a >= (int)landmarks.size() || b >= (int)landmarks.size()) {
			continue;
		}
		if (!isVisible(landmarks[a], style.minVisibility)
			|| !isVisible(landmarks[b], style.minVisibility)) {
			continue;
		}
		ofDrawLine(toScreen(landmarks[a], bounds), toScreen(landmarks[b], bounds));
	}

	ofSetColor(style.jointColor);
	ofFill();
	for (const auto & landmark : landmarks) {
		if (!isVisible(landmark, style.minVisibility)) {
			continue;
		}
		const glm::vec2 point = toScreen(landmark, bounds);
		ofDrawCircle(point, style.jointRadius);
	}
	ofPopStyle();
}

} // namespace

glm::vec2 toScreen(const Landmark & landmark, const ofRectangle & bounds) {
	return glm::vec2(
		bounds.x + landmark.position.x * bounds.width,
		bounds.y + landmark.position.y * bounds.height);
}

void drawPose(const Pose & pose, const ofRectangle & bounds, const DrawStyle & style) {
	drawSkeleton(pose.landmarks, getPoseConnections(), bounds, style);
}

void drawHand(const Hand & hand, const ofRectangle & bounds, const DrawStyle & style) {
	drawSkeleton(hand.landmarks, getHandConnections(), bounds, style);
}

} // namespace ofxMediaPipe
