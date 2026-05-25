cmake_minimum_required(VERSION 3.16)

project(pi_voice_daily_client LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pi_voice_daily_client
  src/audio_device.cpp
  src/main.cpp
  include/audio_device.h
)

find_package(DailyCore REQUIRED)
find_package(Pipecat REQUIRED)
find_package(DailyPipecat REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

target_include_directories(pi_voice_daily_client
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${DAILY_PIPECAT_INCLUDE_DIRS}
  ${PIPECAT_INCLUDE_DIRS}
  ${DAILY_CORE_INCLUDE_DIRS}
  ${PORTAUDIO_INCLUDE_DIRS}
)

target_link_libraries(pi_voice_daily_client
  PRIVATE
  ${DAILY_PIPECAT_LIBRARIES}
  ${PIPECAT_LIBRARIES}
  ${DAILY_CORE_LIBRARIES}
  ${PORTAUDIO_LINK_LIBRARIES}
  CURL::libcurl
)

if(UNIX)
  target_compile_options(pi_voice_daily_client PRIVATE -Wall -Wextra -fno-rtti)
endif()

if(APPLE)
  target_compile_options(pi_voice_daily_client PRIVATE -fvisibility=hidden)

  find_library(CORE_GRAPHICS CoreGraphics)
  find_library(CORE_MEDIA CoreMedia)
  find_library(CORE_AUDIO CoreAudio)
  find_library(CORE_VIDEO CoreVideo)
  find_library(AUDIO_TOOLBOX AudioToolbox)
  find_library(VIDEO_TOOLBOX VideoToolbox)
  find_library(SECURITY Security)
  find_library(FOUNDATION Foundation)
  find_library(APP_KIT AppKit)
  find_library(AVFOUNDATION AVFoundation)
  find_library(METAL Metal)
  find_library(METAL_KIT MetalKit)
  find_library(OPENGL OpenGL)
  find_library(QUARTZ_CORE QuartzCore)

  target_link_libraries(pi_voice_daily_client
    PRIVATE
    ${CORE_GRAPHICS}
    ${CORE_MEDIA}
    ${CORE_AUDIO}
    ${CORE_VIDEO}
    ${AUDIO_TOOLBOX}
    ${VIDEO_TOOLBOX}
    ${SECURITY}
    ${FOUNDATION}
    -ObjC
    ${APP_KIT}
    ${AVFOUNDATION}
    ${METAL}
    ${METAL_KIT}
    ${OPENGL}
    ${QUARTZ_CORE}
  )
endif()
