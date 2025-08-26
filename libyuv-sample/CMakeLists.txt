cmake_minimum_required(VERSION 3.20.0)
project(yuvsimpleapp VERSION 0.1.0 LANGUAGES C CXX)
include(ExternalProject)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb -fPIC")
#add_link_options(-static)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)
#set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(CMAKE_COMMON_ARGS )
if (ANDROID)
    set(CMAKE_COMMON_ARGS
            -DCMAKE_TOOLCHAIN_FILE=/data/devtools/android-sdk/ndk/29.0.13599879/build/cmake/android.toolchain.cmake
            -DANDROID_ABI=arm64-v8a
            -DANDROID_PLATFORM=android-35
    )
endif()


message(WARNING "CMAKE_COMMON_ARGS: ${CMAKE_COMMON_ARGS}")

ExternalProject_Add(libjpeg-turbo
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libjpeg-turbo
    BUILD_ALWAYS 1
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo
        ${CMAKE_COMMON_ARGS}
    BUILD_COMMAND ninja install

)

ExternalProject_Add(zlib
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib
        CMAKE_ARGS
                -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/zlib
                ${CMAKE_COMMON_ARGS}
        BUILD_COMMAND ninja install
)

ExternalProject_Add(libpng
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libpng
        CMAKE_ARGS
                -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libpng
                -DPNG_BUILD_ZLIB=OFF
                ${CMAKE_COMMON_ARGS}
        BUILD_COMMAND ninja install
)

set(JPEG_DIR ${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo)
ExternalProject_Add(libyuv
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libyuv
        CMAKE_ARGS
                -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libyuv
                ${CMAKE_COMMON_ARGS}
                -DJPEG_INCLUDE_DIR=${JPEG_DIR}/include
                -DJPEG_LIBRARY=${JPEG_DIR}/lib/libjpeg.a
        BUILD_COMMAND ninja install
)




set(PNG_DIR ${CMAKE_CURRENT_BINARY_DIR}/libpng)
set(Z_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib)
set(YUV_DIR ${CMAKE_CURRENT_BINARY_DIR}/libyuv)
message(WARNING "PNG_DIR:${PNG_DIR}")
message(WARNING "Z_DIR:${Z_DIR}")
message(WARNING "YUV_DIR:${YUV_DIR}")

include_directories(${Z_DIR}/include)
include_directories(${PNG_DIR}/include)
include_directories(${YUV_DIR}/include)
include_directories(${JPEG_DIR}/include)

link_directories(${Z_DIR}/lib)
link_directories(${PNG_DIR}/lib)
link_directories(${YUV_DIR}/lib)
link_directories(${JPEG_DIR}/lib)


add_executable(yuvsimpleapp main.cpp)

target_link_libraries(yuvsimpleapp PRIVATE
        libyuv.a
        jpeg
        z
        png
)

add_dependencies(yuvsimpleapp  libpng libjpeg-turbo libyuv zlib)

install(TARGETS yuvsimpleapp
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
