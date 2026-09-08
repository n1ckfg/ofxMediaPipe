meta:
ADDON_NAME = ofxMediaPipe
ADDON_DESCRIPTION = Pose landmarking and gesture recognition via the MediaPipe Tasks Vision C API
ADDON_AUTHOR =
ADDON_TAGS = "mediapipe" "ml" "vision" "pose" "hands" "gesture"
ADDON_URL = https://github.com/google-ai-edge/mediapipe

common:
	# MediaPipe Tasks C API headers, laid out as mediapipe/tasks/c/...
	ADDON_INCLUDES = libs/mediapipe/include
	ADDON_INCLUDES += src

	ADDON_SOURCES = src/ofxMediaPipeTypes.cpp
	ADDON_SOURCES += src/ofxMediaPipeInternal.cpp
	ADDON_SOURCES += src/ofxMediaPipePoseLandmarker.cpp
	ADDON_SOURCES += src/ofxMediaPipeGestureRecognizer.cpp
	ADDON_SOURCES += src/ofxMediaPipeTracker.cpp
	ADDON_SOURCES += src/ofxMediaPipeDraw.cpp

	# Internal glue; not part of the addon's public interface.
	ADDON_HEADERS_EXCLUDE = src/ofxMediaPipeInternal.h

linux64:
	# The prebuilt library is dropped in by scripts/build_mediapipe.sh.
	ADDON_LIBS = libs/mediapipe/lib/linux64/libmediapipe_tasks_vision.so

linuxaarch64:
	ADDON_LIBS = libs/mediapipe/lib/linuxaarch64/libmediapipe_tasks_vision.so

osx:
	ADDON_LIBS = libs/mediapipe/lib/osx/libmediapipe_tasks_vision.dylib

	# The dylib records its install name as @rpath/..., so the application has
	# to say where to look. The two layouts differ: make copies addon dylibs
	# beside the .app in bin/, which is three levels up from the executable
	# inside the bundle, while the Xcode projects copy it into the bundle's own
	# Frameworks directory. Naming both keeps either build runnable.
	ADDON_LDFLAGS = -Wl,-rpath,@executable_path/../../..
	ADDON_LDFLAGS += -Wl,-rpath,@executable_path/../Frameworks
