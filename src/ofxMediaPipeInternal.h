#pragma once

// Internal glue between the MediaPipe Tasks C API and the addon's types.
// Not part of the public addon interface -- app code should not include this.

#include "ofxMediaPipeTypes.h"

#include "mediapipe/tasks/c/components/containers/category.h"
#include "mediapipe/tasks/c/components/containers/landmark.h"
#include "mediapipe/tasks/c/core/common.h"
#include "mediapipe/tasks/c/core/mp_status.h"
#include "mediapipe/tasks/c/vision/core/image.h"

#include <string>
#include <vector>

namespace ofxMediaPipe {
namespace internal {

/// Owns an MpImagePtr so it is released on every return path.
class ScopedImage {
public:
	ScopedImage() = default;
	~ScopedImage() { reset(); }

	ScopedImage(const ScopedImage &) = delete;
	ScopedImage & operator=(const ScopedImage &) = delete;

	void reset() {
		if (image != nullptr) {
			MpImageFree(image);
			image = nullptr;
		}
	}

	MpImagePtr get() const { return image; }
	MpImagePtr * addr() { return &image; }

private:
	MpImagePtr image = nullptr;
};

/// Takes ownership of a `char*` error message from the C API and returns it as
/// a std::string, freeing the original. Returns `fallback` if none was set.
std::string takeError(char * errorMsg, const std::string & fallback);

/// Wraps `pixels` as an sRGB/sRGBA MpImage, converting first if the incoming
/// pixel format is neither RGB nor RGBA. `scratch` backs the converted copy and
/// must outlive the returned image.
/// Returns false and fills `outError` if the frame cannot be converted.
bool makeImage(const ofPixels & pixels, ofPixels & scratch, ScopedImage & outImage,
	std::string & outError);

Landmark toLandmark(const NormalizedLandmark & src);
Landmark toLandmark(const ::Landmark & src);
Category toCategory(const ::Category & src);

/// Copies a `Landmarks`/`NormalizedLandmarks` list into a vector.
std::vector<Landmark> toLandmarks(const NormalizedLandmarks & src);
std::vector<Landmark> toLandmarks(const Landmarks & src);

} // namespace internal
} // namespace ofxMediaPipe
