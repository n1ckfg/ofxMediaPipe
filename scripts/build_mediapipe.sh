#!/usr/bin/env bash
#
# Builds the MediaPipe Tasks Vision C API as a single shared library and
# installs it, its headers, and the task models into this addon.
#
# MediaPipe only builds under Bazel, which openFrameworks' Make-based build
# cannot drive, so the library is built once here and linked as a prebuilt
# shared library afterwards. Expect a few hours on a Raspberry Pi 4, and rather
# less on a Mac.
#
# Hosts: Linux (x86_64, aarch64) and macOS (Apple Silicon, Intel).
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
		-h|--help) sed -n '2,13p' "$0"; exit 0 ;;
		*) echo "unknown argument: $1" >&2; exit 1 ;;
	esac
done

ADDON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="${ADDON_DIR}/.build"
SRC_DIR="${WORK_DIR}/mediapipe-src"
BAZEL_BIN="${WORK_DIR}/bazel-${BAZEL_VERSION}"

# The host OS is decided before the architecture. "arm64" means a Raspberry Pi
# under Linux and an Apple Silicon Mac under Darwin; mapping both to
# linuxaarch64 would silently overwrite one platform's library with the other's.
HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"
BAZEL_CONFIG=""

case "${HOST_OS}" in
	Linux)
		case "${HOST_ARCH}" in
			aarch64|arm64) PLATFORM="linuxaarch64"; BAZEL_HOST="linux-arm64" ;;
			x86_64) PLATFORM="linux64"; BAZEL_HOST="linux-x86_64" ;;
			*) echo "unsupported architecture: ${HOST_ARCH}" >&2; exit 1 ;;
		esac
		# ELF: the library is found through DT_SONAME and an $ORIGIN rpath.
		LIB_NAME="libmediapipe_tasks_vision.so"
		;;
	Darwin)
		case "${HOST_ARCH}" in
			arm64) PLATFORM="osx"; BAZEL_HOST="darwin-arm64"; BAZEL_CONFIG="darwin_arm64" ;;
			x86_64) PLATFORM="osx"; BAZEL_HOST="darwin-x86_64"; BAZEL_CONFIG="darwin_x86_64" ;;
			*) echo "unsupported architecture: ${HOST_ARCH}" >&2; exit 1 ;;
		esac
		# Mach-O: the library is found through its install name and an @rpath.
		LIB_NAME="libmediapipe_tasks_vision.dylib"
		;;
	*) echo "unsupported host OS: ${HOST_OS}" >&2; exit 1 ;;
esac

INCLUDE_DIR="${ADDON_DIR}/libs/mediapipe/include"
LIB_DIR="${ADDON_DIR}/libs/mediapipe/lib/${PLATFORM}"
MODEL_DIR="${ADDON_DIR}/libs/mediapipe/models"

# BSD sed (macOS) requires an explicit backup suffix after -i; GNU sed forbids
# one written that way. Every in-place edit below goes through this.
sed_i() {
	if [[ "${HOST_OS}" == "Darwin" ]]; then
		sed -i '' "$@"
	else
		sed -i "$@"
	fi
}

# Keeps a pristine copy of an upstream file so that re-running this script
# patches the original rather than a file it already patched.
reset_to_pristine() {
	local file="$1"
	[[ -f "${file}.orig" ]] || cp "${file}" "${file}.orig"
	cp "${file}.orig" "${file}"
}

echo "==> MediaPipe ${MEDIAPIPE_VERSION} for ${PLATFORM} (${HOST_OS}/${HOST_ARCH}), ${JOBS} parallel jobs"

echo "==> Checking host dependencies"
required_tools=(git curl python3)
if [[ "${HOST_OS}" == "Darwin" ]]; then
	required_tools+=(clang++)
else
	required_tools+=(g++)
fi
for tool in "${required_tools[@]}"; do
	command -v "$tool" >/dev/null || { echo "missing required tool: $tool" >&2; exit 1; }
done

python3 -c "import numpy" 2>/dev/null || {
	if [[ "${HOST_OS}" == "Darwin" ]]; then
		echo "missing python3 numpy (pip3 install numpy)" >&2
	else
		echo "missing python3 numpy (apt install python3-numpy)" >&2
	fi
	exit 1
}

if [[ "${HOST_OS}" == "Darwin" ]]; then
	command -v brew >/dev/null || {
		echo "missing Homebrew, which is how MediaPipe expects to find OpenCV on macOS" >&2
		exit 1
	}
	BREW_PREFIX="$(brew --prefix)"
	[[ -d "${BREW_PREFIX}/opt/opencv/include/opencv4" ]] || {
		echo "missing OpenCV 4 (brew install opencv)" >&2
		exit 1
	}
	echo "    OpenCV 4 at ${BREW_PREFIX}/opt/opencv"
else
	pkg-config --exists opencv4 || {
		echo "missing OpenCV 4 (apt install libopencv-dev)" >&2; exit 1
	}
fi

mkdir -p "${WORK_DIR}"

echo "==> Fetching Bazel ${BAZEL_VERSION} for ${BAZEL_HOST}"
if [[ ! -x "${BAZEL_BIN}" ]]; then
	curl -fL -o "${BAZEL_BIN}" \
		"https://releases.bazel.build/${BAZEL_VERSION}/release/bazel-${BAZEL_VERSION}-${BAZEL_HOST}"
	chmod +x "${BAZEL_BIN}"
fi

echo "==> Fetching MediaPipe sources"
if [[ ! -d "${SRC_DIR}" ]]; then
	git clone --depth 1 --branch "${MEDIAPIPE_VERSION}" \
		https://github.com/google-ai-edge/mediapipe.git "${SRC_DIR}"
fi

echo "==> Pointing MediaPipe at the system OpenCV"
if [[ "${HOST_OS}" == "Darwin" ]]; then
	# third_party/opencv_macos.BUILD ships pointed at an OpenCV 3 installed by
	# an Intel Homebrew. Its own header comment spells out the OpenCV 4 fix:
	# move the header glob to include/opencv4/opencv2 and the include prefix to
	# include/opencv4. Homebrew's version-independent "opt/opencv" symlink is
	# used as the prefix so no Cellar version has to be pinned here.
	OPENCV_BUILD="${SRC_DIR}/third_party/opencv_macos.BUILD"
	reset_to_pristine "${OPENCV_BUILD}"
	sed_i \
		-e 's;^PREFIX = "opt/opencv@3";PREFIX = "opt/opencv";' \
		-e 's;include/opencv2/\*\*/\*\.h\*;include/opencv4/opencv2/**/*.h*;' \
		-e 's;paths\.join(PREFIX, "include/");paths.join(PREFIX, "include/opencv4");' \
		"${OPENCV_BUILD}"

	# The matching repository root. WORKSPACE hardcodes /usr/local, which is
	# wrong on Apple Silicon, where Homebrew lives in /opt/homebrew.
	WORKSPACE_FILE="${SRC_DIR}/WORKSPACE"
	reset_to_pristine "${WORKSPACE_FILE}"
	sed_i \
		-e "s;path = \"/usr/local\",  # e.g. /usr/local/Cellar for HomeBrew;path = \"${BREW_PREFIX}\",  # patched by ofxMediaPipe;" \
		"${WORKSPACE_FILE}"

	grep -q "path = \"${BREW_PREFIX}\"" "${WORKSPACE_FILE}" || {
		echo "failed to point macos_opencv at ${BREW_PREFIX}" >&2; exit 1
	}
else
	# MediaPipe ships third_party/opencv_linux.BUILD with the OpenCV 4 header
	# globs commented out, because it targets the OpenCV 3 layout by default.
	# Debian and Raspberry Pi OS install OpenCV 4 headers under
	# /usr/include/opencv4, with cvconfig.h in the arch-specific directory, so
	# those lines get enabled here.
	OPENCV_BUILD="${SRC_DIR}/third_party/opencv_linux.BUILD"
	reset_to_pristine "${OPENCV_BUILD}"
	ARCH_TRIPLET="$(gcc -dumpmachine)"
	sed_i \
		-e "s;#\"include/${ARCH_TRIPLET}/opencv4/opencv2/cvconfig.h\";\"include/${ARCH_TRIPLET}/opencv4/opencv2/cvconfig.h\";" \
		-e 's;#"include/opencv4/opencv2/\*\*/\*.h\*";"include/opencv4/opencv2/**/*.h*";' \
		-e "s;#\"include/${ARCH_TRIPLET}/opencv4/\";\"include/${ARCH_TRIPLET}/opencv4/\";" \
		-e 's;#"include/opencv4/";"include/opencv4/";' \
		"${OPENCV_BUILD}"
fi

if [[ "${HOST_OS}" == "Darwin" ]]; then
	echo "==> Skipping the CompileTimeString patch (Clang does not need it)"
else
	echo "==> Patching CompileTimeString for GCC"
	# mediapipe/framework/api3/node.h takes a CompileTimeString as a class-type
	# non-type template parameter. GCC requires such a parameter to be
	# copy-constructible; Clang, which upstream builds with, does not. MediaPipe
	# deletes the copy constructor, so building with GCC fails on every
	# calculator that uses api3::Node. Defaulting the copy constructor is safe:
	# every member is const, so the copy is trivial, and assignment stays
	# deleted.
	CTS_HEADER="${SRC_DIR}/mediapipe/framework/deps/compile_time_string.h"
	if grep -q 'CompileTimeString(const CompileTimeString&) = delete;' "${CTS_HEADER}"; then
		sed_i 's;CompileTimeString(const CompileTimeString&) = delete;CompileTimeString(const CompileTimeString\&) = default;' "${CTS_HEADER}"
	fi
fi

echo "==> Adding the combined C API target"
# Upstream ships one shared library per task. Loading two of them would put two
# private copies of the MediaPipe, absl and TFLite runtimes in a single process,
# so the two C APIs are linked into one library instead.
if [[ "${HOST_OS}" == "Darwin" ]]; then
	# Mach-O has no -soname. The equivalent is an install name, and recording it
	# as @rpath lets an app find the dylib inside its own bundle.
	LINK_OPTS="        \"-Wl,-install_name,@rpath/${LIB_NAME}\","
else
	LINK_OPTS="        \"-Wl,-soname=${LIB_NAME}\",
        \"-fvisibility=hidden\","
fi

mkdir -p "${SRC_DIR}/mediapipe/tasks/c/vision/ofx"
cat > "${SRC_DIR}/mediapipe/tasks/c/vision/ofx/BUILD" <<BUILD_FILE
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "${LIB_NAME}",
    linkopts = [
${LINK_OPTS}
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
bazel_args=(build -c opt
	--define MEDIAPIPE_DISABLE_GPU=1
	--action_env PYTHON_BIN_PATH="$(command -v python3)"
	--jobs="${JOBS}"
	--verbose_failures)
# MediaPipe's platform_mappings only resolves an Apple toolchain when --cpu and
# --apple_platform_type are both set, which is what these .bazelrc configs do.
[[ -n "${BAZEL_CONFIG}" ]] && bazel_args+=(--config="${BAZEL_CONFIG}")

cd "${SRC_DIR}"
"${BAZEL_BIN}" "${bazel_args[@]}" "//mediapipe/tasks/c/vision/ofx:${LIB_NAME}"

echo "==> Installing the library"
mkdir -p "${LIB_DIR}"
cp -f "bazel-bin/mediapipe/tasks/c/vision/ofx/${LIB_NAME}" "${LIB_DIR}/"
chmod +x "${LIB_DIR}/${LIB_NAME}"

echo "==> Verifying the exported symbols"
# The addon calls these directly. A library that links but exports nothing shows
# up much later as a wall of undefined symbols when an application links, so it
# is worth catching here.
if [[ "${HOST_OS}" == "Darwin" ]]; then
	exported="$(nm -gU "${LIB_DIR}/${LIB_NAME}" | awk '{print $NF}')"
	symbol_prefix="_"
	echo "    install name: $(otool -D "${LIB_DIR}/${LIB_NAME}" | tail -1)"
else
	exported="$(nm -D --defined-only "${LIB_DIR}/${LIB_NAME}" | awk '{print $NF}')"
	symbol_prefix=""
fi
for symbol in MpPoseLandmarkerCreate MpGestureRecognizerCreate MpImageCreateFromUint8Data MpErrorFree; do
	grep -qx -- "${symbol_prefix}${symbol}" <<< "${exported}" || {
		echo "the built library does not export ${symbol}" >&2; exit 1
	}
done
echo "    all four probe symbols exported"

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
echo "  library: ${LIB_DIR}/${LIB_NAME}"
echo "  headers: ${INCLUDE_DIR}/mediapipe/tasks/c"
echo "  models:  ${MODEL_DIR}"
echo
echo "Next: copy the models into your project's bin/data, then build."
