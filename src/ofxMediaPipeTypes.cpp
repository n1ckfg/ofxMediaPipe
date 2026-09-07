#include "ofxMediaPipeTypes.h"

namespace ofxMediaPipe {

const std::vector<std::pair<int, int>> & getPoseConnections() {
	using namespace PoseLandmarkIndex;
	static const std::vector<std::pair<int, int>> connections{
		// face
		{Nose, LeftEyeInner}, {LeftEyeInner, LeftEye}, {LeftEye, LeftEyeOuter},
		{LeftEyeOuter, LeftEar},
		{Nose, RightEyeInner}, {RightEyeInner, RightEye}, {RightEye, RightEyeOuter},
		{RightEyeOuter, RightEar},
		{MouthLeft, MouthRight},
		// torso
		{LeftShoulder, RightShoulder},
		{LeftShoulder, LeftHip}, {RightShoulder, RightHip}, {LeftHip, RightHip},
		// left arm
		{LeftShoulder, LeftElbow}, {LeftElbow, LeftWrist},
		{LeftWrist, LeftPinky}, {LeftWrist, LeftIndex}, {LeftWrist, LeftThumb},
		{LeftPinky, LeftIndex},
		// right arm
		{RightShoulder, RightElbow}, {RightElbow, RightWrist},
		{RightWrist, RightPinky}, {RightWrist, RightIndex}, {RightWrist, RightThumb},
		{RightPinky, RightIndex},
		// left leg
		{LeftHip, LeftKnee}, {LeftKnee, LeftAnkle},
		{LeftAnkle, LeftHeel}, {LeftHeel, LeftFootIndex}, {LeftAnkle, LeftFootIndex},
		// right leg
		{RightHip, RightKnee}, {RightKnee, RightAnkle},
		{RightAnkle, RightHeel}, {RightHeel, RightFootIndex}, {RightAnkle, RightFootIndex},
	};
	return connections;
}

const std::vector<std::pair<int, int>> & getHandConnections() {
	using namespace HandLandmarkIndex;
	static const std::vector<std::pair<int, int>> connections{
		// thumb
		{Wrist, ThumbCmc}, {ThumbCmc, ThumbMcp}, {ThumbMcp, ThumbIp}, {ThumbIp, ThumbTip},
		// index
		{Wrist, IndexMcp}, {IndexMcp, IndexPip}, {IndexPip, IndexDip}, {IndexDip, IndexTip},
		// middle
		{MiddleMcp, MiddlePip}, {MiddlePip, MiddleDip}, {MiddleDip, MiddleTip},
		// ring
		{RingMcp, RingPip}, {RingPip, RingDip}, {RingDip, RingTip},
		// pinky
		{Wrist, PinkyMcp}, {PinkyMcp, PinkyPip}, {PinkyPip, PinkyDip}, {PinkyDip, PinkyTip},
		// knuckles
		{IndexMcp, MiddleMcp}, {MiddleMcp, RingMcp}, {RingMcp, PinkyMcp},
	};
	return connections;
}

const std::vector<std::string> & getPoseLandmarkNames() {
	static const std::vector<std::string> names{
		"nose",
		"left eye (inner)", "left eye", "left eye (outer)",
		"right eye (inner)", "right eye", "right eye (outer)",
		"left ear", "right ear",
		"mouth (left)", "mouth (right)",
		"left shoulder", "right shoulder",
		"left elbow", "right elbow",
		"left wrist", "right wrist",
		"left pinky", "right pinky",
		"left index", "right index",
		"left thumb", "right thumb",
		"left hip", "right hip",
		"left knee", "right knee",
		"left ankle", "right ankle",
		"left heel", "right heel",
		"left foot index", "right foot index",
	};
	return names;
}

const std::vector<std::string> & getHandLandmarkNames() {
	static const std::vector<std::string> names{
		"wrist",
		"thumb CMC", "thumb MCP", "thumb IP", "thumb tip",
		"index MCP", "index PIP", "index DIP", "index tip",
		"middle MCP", "middle PIP", "middle DIP", "middle tip",
		"ring MCP", "ring PIP", "ring DIP", "ring tip",
		"pinky MCP", "pinky PIP", "pinky DIP", "pinky tip",
	};
	return names;
}

} // namespace ofxMediaPipe
