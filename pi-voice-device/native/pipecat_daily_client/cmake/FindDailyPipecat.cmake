# Locate the Pipecat Daily transport SDK using DAILY_PIPECAT_SDK_PATH.
#
# Required variables:
#   DAILY_PIPECAT_SDK_PATH=/path/to/pipecat-client-cxx-daily
#
# Output variables:
#   DAILY_PIPECAT_FOUND
#   DAILY_PIPECAT_INCLUDE_DIRS
#   DAILY_PIPECAT_LIBRARIES

if(NOT DEFINED ENV{DAILY_PIPECAT_SDK_PATH})
  message(FATAL_ERROR "DAILY_PIPECAT_SDK_PATH must point to pipecat-client-cxx-daily")
endif()

set(DAILY_PIPECAT_ROOT "$ENV{DAILY_PIPECAT_SDK_PATH}")

find_path(DAILY_PIPECAT_INCLUDE_DIR
  NAMES daily_rtvi.h
  PATHS "${DAILY_PIPECAT_ROOT}/include"
)

find_library(DAILY_PIPECAT_LIBRARY
  NAMES daily_pipecat
  PATHS "${DAILY_PIPECAT_ROOT}/lib" "${DAILY_PIPECAT_ROOT}/lib/Release"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  DailyPipecat
  REQUIRED_VARS DAILY_PIPECAT_INCLUDE_DIR DAILY_PIPECAT_LIBRARY
)

set(DAILY_PIPECAT_INCLUDE_DIRS "${DAILY_PIPECAT_INCLUDE_DIR}")
set(DAILY_PIPECAT_LIBRARIES "${DAILY_PIPECAT_LIBRARY}")
