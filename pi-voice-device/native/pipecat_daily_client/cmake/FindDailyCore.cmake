# Locate the Daily Core C++ SDK using DAILY_CORE_PATH.
#
# Required variables:
#   DAILY_CORE_PATH=/path/to/daily-core-sdk
#
# Output variables:
#   DAILY_CORE_FOUND
#   DAILY_CORE_INCLUDE_DIRS
#   DAILY_CORE_LIBRARIES

if(NOT DEFINED ENV{DAILY_CORE_PATH})
  message(FATAL_ERROR "DAILY_CORE_PATH must point to the Daily Core SDK")
endif()

set(DAILY_CORE_ROOT "$ENV{DAILY_CORE_PATH}")

find_path(DAILY_CORE_INCLUDE_DIR
  NAMES daily_core.h
  PATHS "${DAILY_CORE_ROOT}/include"
)

find_library(DAILY_CORE_LIBRARY
  NAMES daily_core
  PATHS "${DAILY_CORE_ROOT}/lib" "${DAILY_CORE_ROOT}/lib/Release"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  DailyCore
  REQUIRED_VARS DAILY_CORE_INCLUDE_DIR DAILY_CORE_LIBRARY
)

set(DAILY_CORE_INCLUDE_DIRS "${DAILY_CORE_INCLUDE_DIR}")
set(DAILY_CORE_LIBRARIES "${DAILY_CORE_LIBRARY}")
