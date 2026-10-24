cmake_minimum_required(VERSION 3.20)

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(RetroSignalChain CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Dépôt https://github.com/AcademySoftwareFoundation/openfx (git clone --recursive)
set(OFX_SDK_DIR "" CACHE PATH "Racine du dépôt openfx")
if(NOT EXISTS "${OFX_SDK_DIR}/include/ofxCore.h")
  message(FATAL_ERROR "Renseignez -DOFX_SDK_DIR=/chemin/vers/openfx")
endif()

file(GLOB OFX_SUPPORT_SRC "${OFX_SDK_DIR}/Support/Library/*.cpp")
find_package(Threads REQUIRED)

add_library(RetroSignalChain MODULE src/RetroPlugin.cpp src/Engine.cpp ${OFX_SUPPORT_SRC})
target_include_directories(RetroSignalChain PRIVATE src "${OFX_SDK_DIR}/include" "${OFX_SDK_DIR}/Support/include")
target_link_libraries(RetroSignalChain PRIVATE Threads::Threads)
target_compile_options(RetroSignalChain PRIVATE -O3 -ffast-math -fvisibility=hidden)
set_target_properties(RetroSignalChain PROPERTIES PREFIX "" SUFFIX ".ofx" CXX_VISIBILITY_PRESET hidden)

if(APPLE)
  set(ARCH_DIR MacOS)
elseif(WIN32)
  set(ARCH_DIR Win64)
else()
  set(ARCH_DIR Linux-x86-64)
endif()

set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/RetroSignalChain.ofx.bundle")
add_custom_command(TARGET RetroSignalChain POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}/Contents/${ARCH_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:RetroSignalChain>" "${BUNDLE_DIR}/Contents/${ARCH_DIR}/RetroSignalChain.ofx")

if(APPLE)
  add_custom_command(TARGET RetroSignalChain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/Info.plist" "${BUNDLE_DIR}/Contents/Info.plist"
    COMMAND codesign --force --deep -s - "${BUNDLE_DIR}")
endif()
