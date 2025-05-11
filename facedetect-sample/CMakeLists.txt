cmake_minimum_required(VERSION 3.31)
project(facedetect_sample)

set(CMAKE_CXX_STANDARD 11)
set(OpenCV_DIR /workspace/github/opencv-4.11.0/build/out-br/lib/cmake/opencv4)

# Find OpenCV, you may need to set OpenCV_DIR variable
# to the absolute path to the directory containing OpenCVConfig.cmake file
# via the command line or GUI
find_package(OpenCV REQUIRED)

if (WIN32 OR MSVC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
elseif (UNIX)
    # 仅查找静态库，强制后缀为 .a
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

    # 如果只是优先查找静态库，保证 .a 后缀在前面即可，把默认的后缀加上
    # set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# If the package has been found, several variables will
# be set, you can find the full list with descriptions
# in the OpenCVConfig.cmake file.
# Print some message showing some of them
message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(facedetect_sample main.cpp)

target_link_libraries(facedetect_sample PRIVATE ${OpenCV_LIBS})
