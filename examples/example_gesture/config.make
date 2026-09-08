################################################################################
# CONFIGURE PROJECT MAKEFILE (optional)
################################################################################

OF_ROOT = ../../../..
# PROJECT_ROOT = .
# PROJECT_DEFINES =

################################################################################
# PROJECT LINKER FLAGS
################################################################################
# libmediapipe_tasks_vision.so is copied next to the binary at build time, so
# look for it there at run time.
PROJECT_LDFLAGS = -Wl,-rpath,'$$ORIGIN'

# PROJECT_CFLAGS =
# PROJECT_OPTIMIZATION_CFLAGS_RELEASE =
# PROJECT_OPTIMIZATION_CFLAGS_DEBUG =
# PROJECT_CXX =
# PROJECT_CC =

################################################################################
# ADDONS  (listed in addons.make)
################################################################################
