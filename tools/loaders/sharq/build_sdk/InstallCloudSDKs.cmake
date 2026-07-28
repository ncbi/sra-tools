# ===========================================================================
# Cloud SDKs Installation CMakeLists.txt
#
# This CMake script automatically downloads, builds, and installs
# AWS SDK for C++ and Google Cloud C++ Client Libraries
# to a local directory (default: ~/local)
#
# Usage:
#   mkdir build && cd build
#   cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/local
#   make -j$(nproc)
#
# ===========================================================================

cmake_minimum_required(VERSION 3.16)
project(CloudSDKsInstaller)

include(ExternalProject)

# Set default install prefix to ~/local if not specified
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/local" CACHE PATH "Install prefix" FORCE)
endif()

message(STATUS "Installing cloud SDKs to: ${CMAKE_INSTALL_PREFIX}")

# Create install directories
file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib" "${CMAKE_INSTALL_PREFIX}/include" "${CMAKE_INSTALL_PREFIX}/bin")

# ===========================================================================
# AWS SDK for C++
# ===========================================================================

ExternalProject_Add(aws-sdk-cpp
    PREFIX ${CMAKE_BINARY_DIR}/aws-sdk-cpp
    GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DBUILD_ONLY=s3
        -DBUILD_SHARED_LIBS=OFF
        -DENABLE_TESTING=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

# ===========================================================================
# Abseil (required for Google Cloud C++)
# ===========================================================================

ExternalProject_Add(abseil-cpp
    PREFIX ${CMAKE_BINARY_DIR}/abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DABSL_BUILD_TESTING=OFF
        -DABSL_PROPAGATE_CXX_STD=ON
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

# ===========================================================================
# nlohmann/json (required for Google Cloud C++)
# ===========================================================================

ExternalProject_Add(nlohmann_json
    PREFIX ${CMAKE_BINARY_DIR}/nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DJSON_BuildTests=OFF
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

# ===========================================================================
# Google Test (required for Google Cloud C++)
# ===========================================================================

ExternalProject_Add(googletest
    PREFIX ${CMAKE_BINARY_DIR}/googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

# ===========================================================================
# Google Cloud C++ Client
# ===========================================================================

ExternalProject_Add(google-cloud-cpp
    PREFIX ${CMAKE_BINARY_DIR}/google-cloud-cpp
    GIT_REPOSITORY https://github.com/googleapis/google-cloud-cpp.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
        -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
        -DBUILD_TESTING=OFF
        -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF
        -DGOOGLE_CLOUD_CPP_ENABLE=storage
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    DEPENDS abseil-cpp nlohmann_json googletest
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

# ===========================================================================
# Top-level target to build all SDKs
# ===========================================================================

add_custom_target(install-cloud-sdks
    DEPENDS aws-sdk-cpp google-cloud-cpp
    COMMENT "All cloud SDKs have been built and installed to ${CMAKE_INSTALL_PREFIX}"
)

# Default target
add_custom_target(all-sdks ALL DEPENDS install-cloud-sdks)
