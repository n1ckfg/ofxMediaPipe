meta:
ADDON_NAME = ofxMediaPipe
ADDON_DESCRIPTION = MediaPipe integration for openFrameworks using MediaPipe Tasks C API
ADDON_AUTHOR = 
ADDON_TAGS = "mediapipe" "ml" "vision" "pose" "face" "hands"
ADDON_URL = https://github.com/google/mediapipe

common:
# MediaPipe Tasks C API headers
ADDON_INCLUDES = libs/mediapipe/include

# Link against libmediapipe shared library (built via cpvrlab/libmediapipe)
# Note: The user must build libmediapipe for their platform and place the .so in libs/mediapipe/lib/<platform>/
ADDON_LDFLAGS = -lmediapipe
ADDON_LDFLAGS += -lopencv_core -lopencv_imgproc -lopencv_videoio
ADDON_LDFLAGS += -labsl_strings -labsl_status -lprotobuf
ADDON_LDFLAGS += -ltensorflow-lite -lflatbuffers

# Source files
ADDON_SRC = src/ofxMediaPipe.cpp
ADDON_SRC += src/FaceLandmarker.cpp
ADDON_SRC += src/HandLandmarker.cpp
ADDON_SRC += src/PoseLandmarker.cpp

# Data (model files)
ADDON_DATA = libs/mediapipe/models/*.task

# Platform-specific library paths (to be set by user or via find_package in future)
# For now, we assume the user will set ADDON_LIBS appropriately in config.make or via pkg-config
# Example for linuxarmv7l:
# linuxarmv7l:
#   ADDON_LIBS = libs/mediapipe/lib/linuxarmv7l/libmediapipe.so

# Instructions for user:
# 1. Build libmediapipe using cpvrlab/libmediapipe for your platform.
# 2. Copy the generated libmediapipe.so to libs/mediapipe/lib/<your_platform>/
# 3. Download the desired .task model files (e.g., pose_landmarker.task) and place them in libs/mediapipe/models/
# 4. Update the addon_config.mk or your project's config.make to point to the correct library path if needed.

# Note: This addon is a work in progress and requires the user to build the MediaPipe shared library first.