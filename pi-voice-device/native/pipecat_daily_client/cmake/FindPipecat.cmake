# Locate the base Pipecat C++ SDK using PIPECAT_SDK_PATH.
#
# Required variables:
#   PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx
#
# Output variables:
#   PIPECAT_FOUND
#   PIPECAT_INCLUDE_DIRS
#   PIPECAT_LIBRARIES

if(NOT DEFINED ENV{PIPECAT_SDK_PATH})
  message(FATAL_ERROR "PIPECAT_SDK_PATH must point to pipecat-client-cxx")
endif()

set(PIPECAT_ROOT "$ENV{PIPECAT_SDK_PATH}")

find_path(PIPECAT_INCLUDE_DIR
  NAMES rtvi.h
  PATHS "${PIPECAT_ROOT}/include"
)

find_library(PIPECAT_LIBRARY
  NAMES pipecat
  PATHS "${PIPECAT_ROOT}/lib" "${PIPECAT_ROOT}/lib/Release"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Pipecat
  REQUIRED_VARS PIPECAT_INCLUDE_DIR PIPECAT_LIBRARY
)

set(PIPECAT_INCLUDE_DIRS "${PIPECAT_INCLUDE_DIR}")
set(PIPECAT_LIBRARIES "${PIPECAT_LIBRARY}")
