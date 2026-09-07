#pragma once

#include "ofMain.h"

#include <string>
#include <vector>

namespace ofxMediaPipe {

/// A single detected point. `position.xy` is normalized to [0,1] over the input
/// image; `position.z` is a model-relative depth (roughly in the same units as
/// x, smaller means closer to the camera). World landmarks use metres instead,
/// with the origin at the subject's centre.
struct Landmark {
	glm::vec3 position{0.f, 0.f, 0.f};

	/// Whether the point is on screen rather than occluded/out of frame.
	/// `hasVisibility` is false when the model does not report it.
	bool hasVisibility = false;
	float visibility = 0.f;

	bool hasPresence = false;
	float presence = 0.f;
};

/// One entry of a classifier result (a gesture label, or handedness).
struct Category {
	int index = -1;
	float score = 0.f;
	std::string categoryName;
	std::string displayName;

	/// Human-readable name, preferring the localized display name.
	const std::string & label() const {
		return displayName.empty() ? categoryName : displayName;
	}

	bool empty() const { return categoryName.empty() && displayName.empty(); }
};

/// One detected body.
struct Pose {
	/// 33 BlazePose landmarks in normalized image coordinates.
	std::vector<Landmark> landmarks;
	/// The same 33 landmarks in metric world coordinates, hip-centred.
	std::vector<Landmark> worldLandmarks;
};

/// One detected hand, with its recognized gesture.
struct Hand {
	/// 21 hand landmarks in normalized image coordinates.
	std::vector<Landmark> landmarks;
	/// The same 21 landmarks in metric world coordinates, wrist-centred.
	std::vector<Landmark> worldLandmarks;

	/// Highest-scoring gesture. One of the canned categories: None,
	/// Closed_Fist, Open_Palm, Pointing_Up, Thumb_Down, Thumb_Up, Victory,
	/// ILoveYou.
	Category gesture;
	/// "Left" or "Right", as seen from the person, not the camera.
	Category handedness;
};

/// Indices into Pose::landmarks, in MediaPipe's BlazePose order.
namespace PoseLandmarkIndex {
enum Index {
	Nose = 0,
	LeftEyeInner = 1, LeftEye = 2, LeftEyeOuter = 3,
	RightEyeInner = 4, RightEye = 5, RightEyeOuter = 6,
	LeftEar = 7, RightEar = 8,
	MouthLeft = 9, MouthRight = 10,
	LeftShoulder = 11, RightShoulder = 12,
	LeftElbow = 13, RightElbow = 14,
	LeftWrist = 15, RightWrist = 16,
	LeftPinky = 17, RightPinky = 18,
	LeftIndex = 19, RightIndex = 20,
	LeftThumb = 21, RightThumb = 22,
	LeftHip = 23, RightHip = 24,
	LeftKnee = 25, RightKnee = 26,
	LeftAnkle = 27, RightAnkle = 28,
	LeftHeel = 29, RightHeel = 30,
	LeftFootIndex = 31, RightFootIndex = 32,
	Count = 33
};
}

/// Indices into Hand::landmarks, in MediaPipe's hand order.
namespace HandLandmarkIndex {
enum Index {
	Wrist = 0,
	ThumbCmc = 1, ThumbMcp = 2, ThumbIp = 3, ThumbTip = 4,
	IndexMcp = 5, IndexPip = 6, IndexDip = 7, IndexTip = 8,
	MiddleMcp = 9, MiddlePip = 10, MiddleDip = 11, MiddleTip = 12,
	RingMcp = 13, RingPip = 14, RingDip = 15, RingTip = 16,
	PinkyMcp = 17, PinkyPip = 18, PinkyDip = 19, PinkyTip = 20,
	Count = 21
};
}

/// Bone pairs for drawing a skeleton, as index pairs into the landmark vector.
const std::vector<std::pair<int, int>> & getPoseConnections();
const std::vector<std::pair<int, int>> & getHandConnections();

/// Human-readable landmark names, indexed to match the enums above.
const std::vector<std::string> & getPoseLandmarkNames();
const std::vector<std::string> & getHandLandmarkNames();

} // namespace ofxMediaPipe
