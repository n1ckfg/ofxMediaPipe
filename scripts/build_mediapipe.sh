#!/usr/bin/env bash
#
# Builds the MediaPipe Tasks Vision C API as a single shared library and
# installs it, its headers, and the task models into this addon.
#
# MediaPipe only builds under Bazel, which openFrameworks' Make-based build
# cannot drive, so the library is built once here and linked as a prebuilt .so
# afterwards. Expect a few hours on a Raspberry Pi 4.
#
# Usage: scripts/build_mediapipe.sh [--version v0.10.35] [--jobs 3] [--keep-src]

set -euo pipefail

MEDIAPIPE_VERSION="v0.10.35"
BAZEL_VERSION="7.4.1"
JOBS="3"
KEEP_SRC=0

while [[ $# -gt 0 ]]; do
	case "$1" in
		--version) MEDIAPIPE_VERSION="$2"; shift 2 ;;
		--jobs) JOBS="$2"; shift 2 ;;
		--keep-src) KEEP_SRC=1; shift ;;
		-h|--help) sed -n '2,12p' "$0"; exit 0 ;;
		*) echo "unknown argument: $1" >&2; exit 1 ;;
	esac
done

ADDON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ADDON_DIR}/.build"
SRC_DIR="${WORK_DIR}/mediapipe-src"
BAZEL_BIN="${WORK_DIR}/bazel-${BAZEL_VERSION}"

case "$(uname -m)" in
	aarch64|arm64) PLATFORM="linuxaarch64"; BAZEL_ARCH="arm64" ;;
	x86_64) PLATFORM="linux64"; BAZEL_ARCH="x86_64" ;;
	*) echo "unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac

INCLUDE_DIR="${ADDON_DIR}/libs/mediapipe/include"
LIB_DIR="${ADDON_DIR}/libs/mediapipe/lib/${PLATFORM}"
MODEL_DIR="${ADDON_DIR}/libs/mediapipe/models"

echo "==> MediaPipe ${MEDIAPIPE_VERSION} for ${PLATFORM}, ${JOBS} parallel jobs"

echo "==> Checking host dependencies"
for tool in git curl python3 g++; do
	command -v "$tool" >/dev/null || { echo "missing required tool: $tool" >&2; exit 1; }
done
python3 -c "import numpy" 2>/dev/null || {
	echo "missing python3 numpy (apt install python3-numpy)" >&2; exit 1
}
pkg-config --exists opencv4 || {
	echo "missing OpenCV 4 (apt install libopencv-dev)" >&2; exit 1
}

mkdir -p "${WORK_DIR}"

echo "==> Fetching Bazel ${BAZEL_VERSION}"
if [[ ! -x "${BAZEL_BIN}" ]]; then
	curl -fL -o "${BAZEL_BIN}" \
		"https://releases.bazel.build/${BAZEL_VERSION}/release/bazel-${BAZEL_VERSION}-linux-${BAZEL_ARCH}"
	chmod +x "${BAZEL_BIN}"
fi

echo "==> Fetching MediaPipe sources"
if [[ ! -d "${SRC_DIR}" ]]; then
	git clone --depth 1 --branch "${MEDIAPIPE_VERSION}" \
		https://github.com/google-ai-edge/mediapipe.git "${SRC_DIR}"
fi

echo "==> Pointing MediaPipe at the system OpenCV"
# MediaPipe ships third_party/opencv_linux.BUILD with the OpenCV 4 header globs
# commented out, because it targets the OpenCV 3 layout by default. Debian and
# Raspberry Pi OS install OpenCV 4 headers under /usr/include/opencv4, with
# cvconfig.h in the arch-specific directory, so those lines get enabled here.
OPENCV_BUILD="${SRC_DIR}/third_party/opencv_linux.BUILD"
if [[ ! -f "${OPENCV_BUILD}.orig" ]]; then
	cp "${OPENCV_BUILD}" "${OPENCV_BUILD}.orig"
fi
cp "${OPENCV_BUILD}.orig" "${OPENCV_BUILD}"
ARCH_TRIPLET="$(gcc -dumpmachine)"
sed -i \
	-e "s;#\"include/${ARCH_TRIPLET}/opencv4/opencv2/cvconfig.h\";\"include/${ARCH_TRIPLET}/opencv4/opencv2/cvconfig.h\";" \
	-e 's;#"include/opencv4/opencv2/\*\*/\*.h\*";"include/opencv4/opencv2/**/*.h*";' \
	-e "s;#\"include/${ARCH_TRIPLET}/opencv4/\";\"include/${ARCH_TRIPLET}/opencv4/\";" \
	-e 's;#"include/opencv4/";"include/opencv4/";' \
	"${OPENCV_BUILD}"

echo "==> Patching CompileTimeString for GCC"
# mediapipe/framework/api3/node.h takes a CompileTimeString as a class-type
# non-type template parameter. GCC requires such a parameter to be
# copy-constructible; Clang, which upstream builds with, does not. MediaPipe
# deletes the copy constructor, so building with GCC fails on every calculator
# that uses api3::Node. Defaulting the copy constructor is safe: every member is
# const, so the copy is trivial, and assignment stays deleted.
CTS_HEADER="${SRC_DIR}/mediapipe/framework/deps/compile_time_string.h"
if grep -q 'CompileTimeString(const CompileTimeString&) = delete;' "${CTS_HEADER}"; then
	sed -i 's;CompileTimeString(const CompileTimeString&) = delete;CompileTimeString(const CompileTimeString\&) = default;' "${CTS_HEADER}"
fi

echo "==> Adding the combined C API target"
# Upstream ships one .so per task. Loading two of them would put two private
# copies of the MediaPipe, absl and TFLite runtimes in a single process, so the
# two C APIs are linked into one library instead.
mkdir -p "${SRC_DIR}/mediapipe/tasks/c/vision/ofx"
cat > "${SRC_DIR}/mediapipe/tasks/c/vision/ofx/BUILD" <<'BUILD_FILE'
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "libmediapipe_tasks_vision.so",
    linkopts = [
        "-Wl,-soname=libmediapipe_tasks_vision.so",
        "-fvisibility=hidden",
    ],
    linkshared = True,
    tags = ["manual", "nobuilder", "notap"],
    deps = [
        "//mediapipe/tasks/c/vision/gesture_recognizer:gesture_recognizer_c_lib",
        "//mediapipe/tasks/c/vision/pose_landmarker:pose_landmarker_c_lib",
    ],
)
BUILD_FILE

echo "==> Building (this is the slow part)"
cd "${SRC_DIR}"
"${BAZEL_BIN}" build -c opt \
	--define MEDIAPIPE_DISABLE_GPU=1 \
	--action_env PYTHON_BIN_PATH="$(command -v python3)" \
	--jobs="${JOBS}" \
	--verbose_failures \
	//mediapipe/tasks/c/vision/ofx:libmediapipe_tasks_vision.so

echo "==> Installing the library"
mkdir -p "${LIB_DIR}"
cp -f "bazel-bin/mediapipe/tasks/c/vision/ofx/libmediapipe_tasks_vision.so" "${LIB_DIR}/"
chmod +x "${LIB_DIR}/libmediapipe_tasks_vision.so"

echo "==> Installing headers"
rm -rf "${INCLUDE_DIR}/mediapipe"
while IFS= read -r -d '' header; do
	relative="${header#./}"
	mkdir -p "${INCLUDE_DIR}/mediapipe/tasks/c/$(dirname "${relative}")"
	cp "${header}" "${INCLUDE_DIR}/mediapipe/tasks/c/${relative}"
done < <(cd "${SRC_DIR}/mediapipe/tasks/c" && find . -name '*.h' -not -name '*_converter.h' -print0)

cd "${ADDON_DIR}"

echo "==> Downloading models"
mkdir -p "${MODEL_DIR}"
download_model() {
	local name="$1" url="$2"
	if [[ -s "${MODEL_DIR}/${name}" ]]; then
		echo "    ${name} already present"
	else
		echo "    ${name}"
		curl -fL -o "${MODEL_DIR}/${name}" "${url}"
	fi
}
download_model "pose_landmarker_lite.task" \
	"https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/latest/pose_landmarker_lite.task"
download_model "gesture_recognizer.task" \
	"https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/latest/gesture_recognizer.task"

if [[ "${KEEP_SRC}" -eq 0 ]]; then
	echo "==> Reclaiming build space (pass --keep-src to skip)"
	"${BAZEL_BIN}" --output_base="$(cd "${SRC_DIR}" && "${BAZEL_BIN}" info output_base)" clean --expunge >/dev/null 2>&1 || true
fi

echo
echo "Done."
echo "  library: ${LIB_DIR}/libmediapipe_tasks_vision.so"
echo "  headers: ${INCLUDE_DIR}/mediapipe/tasks/c"
echo "  models:  ${MODEL_DIR}"
echo
echo "Next: copy the models into your project's bin/data, then run 'make'."
