# #######################################################################
# Project setup -- only needed if device support is a stand-alone build
# We recommend that the support module be built in-tree with the driver.
# #######################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyAfedri CXX)

# enable_testing()

# select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

# #######################################################################
# Header and library resources needed to communicate with the device.
# These may be found within the build tree or in an external project.
# #######################################################################
set(AFEDRI_INCLUDE_DIRS "")
set(AFEDRI_LIBRARIES "")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
endif()

# network libraries
if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif(WIN32)

# #######################################################################
# build the module
# #######################################################################
find_package(SoapySDR CONFIG)

if(NOT SoapySDR_FOUND)
  message(WARNING "SoapySDR development files not found - skipping support")
  return()
endif()

find_package(Threads REQUIRED)

include_directories(${AFEDRI_INCLUDE_DIRS})
include_directories("src/utils")
SOAPY_SDR_MODULE_UTIL(
  TARGET afedriDevice
  SOURCES
  src/afedri_driver/device_constructor.cpp
  src/afedri_driver/antenna.cpp
  src/afedri_driver/registration.cpp
  src/afedri_driver/settings.cpp
  src/afedri_driver/streaming.cpp
  src/afedri_driver/gain.cpp
  src/afedri_driver/frequency.cpp
  src/afedri_driver/sample_rate.cpp
  src/afedri_driver/soapy_afedri.hpp
  src/afedri_driver/helpers.cpp
  src/utils/simple_tcp_communicator.cpp
  src/utils/simple_tcp_communicator.hpp
  src/utils/afedri_control.cpp
  src/utils/afedri_control.hpp
  src/utils/buffer.hpp
  src/utils/buffer.cpp
  src/utils/udp_rx.cpp
  src/utils/udp_rx.hpp

  LIBRARIES ${AFEDRI_LIBRARIES}
)

target_link_libraries(afedriDevice PRIVATE Threads::Threads)

if(WIN32)
  target_link_libraries(afedriDevice PRIVATE ws2_32)
endif(WIN32)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/probes")
  add_subdirectory(src/probes)
endif()
