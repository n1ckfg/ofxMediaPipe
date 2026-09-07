#include "ofxMediaPipeInternal.h"

#include <cstdlib>

namespace ofxMediaPipe {
namespace internal {

std::string takeError(char * errorMsg, const std::string & fallback) {
	if (errorMsg == nullptr) {
		return fallback;
	}
	std::string message(errorMsg);
	MpErrorFree(errorMsg);
	return message.empty() ? fallback : message;
}

bool makeImage(const ofPixels & pixels, ofPixels & scratch, ScopedImage & outImage,
	std::string & outError) {

	if (!pixels.isAllocated() || pixels.getWidth() <= 0 || pixels.getHeight() <= 0) {
		outError = "frame is empty";
		return false;
	}

	// MediaPipe accepts RGB and RGBA only; anything else (grayscale from a mono
	// sensor, BGRA from some capture paths) gets converted once into `scratch`.
	const ofPixels * source = &pixels;
	const ofPixelFormat format = pixels.getPixelFormat();
	if (format != OF_PIXELS_RGB && format != OF_PIXELS_RGBA) {
		scratch = pixels;
		scratch.setImageType(OF_IMAGE_COLOR);
		source = &scratch;
	}

	const MpImageFormat mpFormat
		= (source->getPixelFormat() == OF_PIXELS_RGBA) ? kMpImageFormatSrgba : kMpImageFormatSrgb;

	char * error = nullptr;
	const MpStatus status = MpImageCreateFromUint8Data(
		mpFormat,
		static_cast<int>(source->getWidth()),
		static_cast<int>(source->getHeight()),
		source->getData(),
		static_cast<int>(source->size()),
		outImage.addr(),
		&error);

	if (status != kMpOk) {
		outError = takeError(error, "MpImageCreateFromUint8Data failed");
		return false;
	}
	return true;
}

Landmark toLandmark(const NormalizedLandmark & src) {
	Landmark out;
	out.position = glm::vec3(src.x, src.y, src.z);
	out.hasVisibility = src.has_visibility;
	out.visibility = src.visibility;
	out.hasPresence = src.has_presence;
	out.presence = src.presence;
	return out;
}

Landmark toLandmark(const ::Landmark & src) {
	Landmark out;
	out.position = glm::vec3(src.x, src.y, src.z);
	out.hasVisibility = src.has_visibility;
	out.visibility = src.visibility;
	out.hasPresence = src.has_presence;
	out.presence = src.presence;
	return out;
}

Category toCategory(const ::Category & src) {
	Category out;
	out.index = src.index;
	out.score = src.score;
	if (src.category_name != nullptr) {
		out.categoryName = src.category_name;
	}
	if (src.display_name != nullptr) {
		out.displayName = src.display_name;
	}
	return out;
}

std::vector<Landmark> toLandmarks(const NormalizedLandmarks & src) {
	std::vector<Landmark> out;
	if (src.landmarks == nullptr) {
		return out;
	}
	out.reserve(src.landmarks_count);
	for (uint32_t i = 0; i < src.landmarks_count; ++i) {
		out.push_back(toLandmark(src.landmarks[i]));
	}
	return out;
}

std::vector<Landmark> toLandmarks(const Landmarks & src) {
	std::vector<Landmark> out;
	if (src.landmarks == nullptr) {
		return out;
	}
	out.reserve(src.landmarks_count);
	for (uint32_t i = 0; i < src.landmarks_count; ++i) {
		out.push_back(toLandmark(src.landmarks[i]));
	}
	return out;
}

} // namespace internal
} // namespace ofxMediaPipe
