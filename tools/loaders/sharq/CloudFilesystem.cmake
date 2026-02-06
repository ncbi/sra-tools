# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

# ===========================================================================
# Cloud Filesystem Support Configuration (Native SDKs)
#
# This module provides options for enabling cloud storage support in SharQ
# using native AWS and Google Cloud SDKs.
#
# Options:
#   SHARQ_ENABLE_CLOUD - Enable cloud support (S3, GCS) using native SDKs
#
# Usage:
#   cmake -DSHARQ_ENABLE_CLOUD=ON ..
#
# ===========================================================================

option(SHARQ_ENABLE_CLOUD "Enable cloud storage support (S3, GCS) using native SDKs" ON)

# Path hints for finding locally installed SDKs (e.g., ~/local)
set(SHARQ_CLOUD_SDK_PREFIX "$ENV{HOME}/local" CACHE PATH "Custom install prefix for cloud SDKs (e.g., $ENV{HOME}/local)")

# Variables to track what we found
set(SHARQ_CLOUD_LIBS "")
set(SHARQ_CLOUD_INCLUDES "")
set(SHARQ_CLOUD_DEFINITIONS "")
set(SHARQ_CLOUD_AVAILABLE FALSE)
set(SHARQ_HAS_AWS FALSE)
set(SHARQ_HAS_GCS FALSE)

find_library(CRC32C NAMES crc32c.a ${SHARQ_CLOUD_SDK_PREFIX}/lib ${SHARQ_CLOUD_SDK_PREFIX}/lib64)

if(SHARQ_ENABLE_CLOUD)
    # Add custom prefix to CMake search paths if specified
    if(SHARQ_CLOUD_SDK_PREFIX)
        message(STATUS "Using cloud SDK prefix: ${SHARQ_CLOUD_SDK_PREFIX}")
        list(PREPEND CMAKE_PREFIX_PATH "${SHARQ_CLOUD_SDK_PREFIX}")
        list(PREPEND CMAKE_LIBRARY_PATH "${SHARQ_CLOUD_SDK_PREFIX}/lib" "${SHARQ_CLOUD_SDK_PREFIX}/lib64")
        list(PREPEND CMAKE_INCLUDE_PATH "${SHARQ_CLOUD_SDK_PREFIX}/include")
    endif()
    message(STATUS "=== Cloud Storage Support Configuration ===")
    message(STATUS "Looking for native cloud SDKs...")

    # =========================================================================
    # AWS SDK for C++
    # =========================================================================
    find_package(AWSSDK COMPONENTS s3 QUIET)
    if(AWSSDK_FOUND)
        message(STATUS "Found AWS SDK version ${AWSSDK_VERSION}")
        # Filter out shared libcrc32c from AWSSDK_LINK_LIBRARIES to prevent dynamic linking
        list(REMOVE_ITEM AWSSDK_LINK_LIBRARIES "libcrc32c.so.1.1.0")
        list(REMOVE_ITEM AWSSDK_LINK_LIBRARIES "crc32c")
        list(APPEND SHARQ_CLOUD_LIBS ${AWSSDK_LINK_LIBRARIES})
        list(APPEND SHARQ_CLOUD_INCLUDES ${AWSSDK_INCLUDE_DIRS})

        # Find static libcrc32c library for static linking
        if(CRC32C)
            message(STATUS "  Found static libcrc32c: libcrc32c.a ${CRC32C}")
            # Add static libcrc32c immediately after AWS libraries to resolve dependencies
            list(APPEND SHARQ_CLOUD_LIBS ${CRC32C})
            message(STATUS "  Added static libcrc32c after AWS SDK libraries")
            set(SHARQ_CRC32C_STATIC_AVAILABLE TRUE)
        else()
            message(WARNING "Static libcrc32c library not found at libcrc32c.a")
            set(SHARQ_CRC32C_STATIC_AVAILABLE FALSE)
        endif()
        set(SHARQ_HAS_AWS TRUE)

    else()
        # Try alternative detection for AWS SDK (for custom installations)
        set(_aws_search_hints "")
        if(SHARQ_CLOUD_SDK_PREFIX)
            list(APPEND _aws_search_hints "${SHARQ_CLOUD_SDK_PREFIX}/lib" "${SHARQ_CLOUD_SDK_PREFIX}/lib64")
        endif()

        find_library(AWS_S3_LIB NAMES aws-cpp-sdk-s3 HINTS ${_aws_search_hints})
        find_library(AWS_CORE_LIB NAMES aws-cpp-sdk-core HINTS ${_aws_search_hints})
        find_library(AWS_CRT_LIB NAMES aws-crt-cpp HINTS ${_aws_search_hints})
        find_path(AWS_INCLUDE_DIR NAMES aws/s3/S3Client.h
                  HINTS "${SHARQ_CLOUD_SDK_PREFIX}/include")

        if(AWS_S3_LIB AND AWS_CORE_LIB AND AWS_INCLUDE_DIR)
            message(STATUS "Found AWS SDK (manual detection)")
            message(STATUS "  S3 lib: ${AWS_S3_LIB}")
            message(STATUS "  Core lib: ${AWS_CORE_LIB}")
            message(STATUS "  Include: ${AWS_INCLUDE_DIR}")

            # Get all AWS C runtime libraries for proper static linking
            file(GLOB AWS_C_LIBS "${SHARQ_CLOUD_SDK_PREFIX}/lib64/libaws-c-*.a"
                                 "${SHARQ_CLOUD_SDK_PREFIX}/lib/libaws-c-*.a")
            file(GLOB AWS_S2N_LIBS "${SHARQ_CLOUD_SDK_PREFIX}/lib64/libs2n*.a"
                                   "${SHARQ_CLOUD_SDK_PREFIX}/lib/libs2n*.a")

            # Find static libcrc32c library
            if(EXISTS "libcrc32c.a")
                message(STATUS "  Found static libcrc32c: libcrc32c.a")
                set(SHARQ_CRC32C_STATIC_AVAILABLE TRUE)
            else()
                message(WARNING "Static libcrc32c library not found at ...")
                set(SHARQ_CRC32C_STATIC_AVAILABLE FALSE)
            endif()

            list(APPEND SHARQ_CLOUD_LIBS ${AWS_S3_LIB} ${AWS_CORE_LIB})
            if(AWS_CRT_LIB)
                list(APPEND SHARQ_CLOUD_LIBS ${AWS_CRT_LIB})
            endif()
            list(APPEND SHARQ_CLOUD_LIBS ${AWS_C_LIBS} ${AWS_S2N_LIBS})

            # Add static libcrc32c immediately after AWS C libraries to resolve dependencies
            if(EXISTS "libcrc32c.a")
                list(APPEND SHARQ_CLOUD_LIBS "libcrc32c.a")
                message(STATUS "  Added static libcrc32c after AWS C libraries")
            endif()

            list(APPEND SHARQ_CLOUD_LIBS curl ssl crypto)
            list(APPEND SHARQ_CLOUD_INCLUDES ${AWS_INCLUDE_DIR})
            set(SHARQ_HAS_AWS TRUE)
        else()
            message(STATUS "AWS SDK not found. S3 support will not be available.")
            message(STATUS "  To enable S3 support, install AWS SDK for C++:")
            message(STATUS "    git clone --recurse-submodules https://github.com/aws/aws-sdk-cpp")
            message(STATUS "    cd aws-sdk-cpp && mkdir build && cd build")
            message(STATUS "    cmake .. -DBUILD_ONLY=\"s3\" -DCMAKE_INSTALL_PREFIX=\$HOME/local")
            message(STATUS "    make -j\$(nproc) && make install")
        endif()
    endif()

    # =========================================================================
    # Google Cloud C++ Client Libraries
    # =========================================================================
    # Skip find_package to avoid shared library dependencies, use manual detection
    # find_package(google_cloud_cpp_storage QUIET)
    # if(google_cloud_cpp_storage_FOUND)
    #     message(STATUS "Found Google Cloud Storage C++ client")
    #     list(APPEND SHARQ_CLOUD_LIBS google-cloud-cpp::storage)
    #     set(SHARQ_HAS_GCS TRUE)
    # else()
        # Try alternative detection for custom installations
        set(_gcs_search_hints "")
        if(SHARQ_CLOUD_SDK_PREFIX)
            list(APPEND _gcs_search_hints "${SHARQ_CLOUD_SDK_PREFIX}/lib" "${SHARQ_CLOUD_SDK_PREFIX}/lib64")
        endif()

        find_library(GCS_STORAGE_LIB NAMES google_cloud_cpp_storage HINTS ${_gcs_search_hints})
        find_library(GCS_REST_LIB NAMES google_cloud_cpp_rest_internal HINTS ${_gcs_search_hints})
        find_library(GCS_COMMON_LIB NAMES google_cloud_cpp_common HINTS ${_gcs_search_hints})
        find_path(GCS_INCLUDE_DIR NAMES google/cloud/storage/client.h
                  HINTS "${SHARQ_CLOUD_SDK_PREFIX}/include")

        if(GCS_STORAGE_LIB AND GCS_COMMON_LIB AND GCS_INCLUDE_DIR)
            message(STATUS "Found Google Cloud Storage SDK (manual detection)")
            message(STATUS "  Storage lib: ${GCS_STORAGE_LIB}")
            message(STATUS "  Common lib: ${GCS_COMMON_LIB}")
            message(STATUS "  Include: ${GCS_INCLUDE_DIR}")

            # GCS requires abseil libraries for static linking
            message(STATUS "  Include ABSL: ${SHARQ_CLOUD_SDK_PREFIX}/lib64/libabsl_*.a")

            file(GLOB ABSL_LIBS "${SHARQ_CLOUD_SDK_PREFIX}/lib64/libabsl_*.a"
                                "${SHARQ_CLOUD_SDK_PREFIX}/lib/libabsl_*.a")

            message(STATUS "  ABSL_LIBS: ${ABSL_LIBS}")

            # Find static libcrc32c library for static linking (Google Cloud)
            if(CRC32C)
                set(CRC32C_STATIC_LIB "${CRC32C}")
                message(STATUS "  Found static libcrc32c: ${CRC32C_STATIC_LIB}")
            else()
                message(WARNING "Static libcrc32c library not found at ...")
            endif()

            list(APPEND SHARQ_CLOUD_LIBS ${GCS_STORAGE_LIB})
            if(GCS_REST_LIB)
                list(APPEND SHARQ_CLOUD_LIBS ${GCS_REST_LIB})
            endif()
            list(APPEND SHARQ_CLOUD_LIBS ${GCS_COMMON_LIB})
            list(APPEND SHARQ_CLOUD_LIBS ${ABSL_LIBS})

            # Add static libcrc32c for GCS if available
            if(CRC32C_STATIC_LIB)
                list(APPEND SHARQ_CLOUD_LIBS ${CRC32C_STATIC_LIB})
                message(STATUS "  Added static libcrc32c for GCS")
            endif()

            list(APPEND SHARQ_CLOUD_LIBS curl ssl crypto pthread)
            list(APPEND SHARQ_CLOUD_INCLUDES ${GCS_INCLUDE_DIR})
            set(SHARQ_HAS_GCS TRUE)
        else()
            message(STATUS "Google Cloud Storage SDK not found. GCS support will not be available.")
            message(STATUS "  To enable GCS support, install Google Cloud C++ client to \$HOME/local.")
            message(STATUS "  See the installation instructions at the end of CloudFilesystem.cmake")
        endif()
    # endif()

    # =========================================================================
    # Summary and Final Configuration
    # =========================================================================
    if(SHARQ_HAS_AWS OR SHARQ_HAS_GCS)
        list(APPEND SHARQ_CLOUD_DEFINITIONS "SHARQ_USE_NATIVE_CLOUD")
        set(SHARQ_CLOUD_AVAILABLE TRUE)
        message(STATUS "")
        message(STATUS "Cloud storage support: ENABLED")
        message(STATUS "  AWS S3:  ${SHARQ_HAS_AWS}")
        message(STATUS "  GCS:     ${SHARQ_HAS_GCS}")
        message(STATUS "  Libraries: ${SHARQ_CLOUD_LIBS}")
    else()
        message(WARNING "Cloud storage support requested but no suitable libraries found.")
        message(STATUS "Install AWS SDK and/or Google Cloud SDK to enable cloud support.")
    endif()

    message(STATUS "===========================================")
endif()

# ===========================================================================
# Helper function to add cloud support to a target
# ===========================================================================

function(sharq_add_cloud_support target)
    if(SHARQ_CLOUD_AVAILABLE)
        # Filter out shared libcrc32c from cloud libraries to prevent dynamic linking
        list(REMOVE_ITEM SHARQ_CLOUD_LIBS "libcrc32c.so.1.1.0")
        list(REMOVE_ITEM SHARQ_CLOUD_LIBS "crc32c")
        target_link_libraries(${target} PRIVATE ${SHARQ_CLOUD_LIBS})
        if(SHARQ_CLOUD_INCLUDES)
            target_include_directories(${target} PRIVATE ${SHARQ_CLOUD_INCLUDES})
        endif()
        target_compile_definitions(${target} PRIVATE ${SHARQ_CLOUD_DEFINITIONS})

        message(STATUS "Added cloud support to target: ${target}")
        message(STATUS "  AWS S3: ${SHARQ_HAS_AWS}, GCS: ${SHARQ_HAS_GCS}")
    endif()
endfunction()

# ===========================================================================
# Installation instructions (no sudo required)
# ===========================================================================

# Create local install directory:
#   export SDK=<local path>
#   mkdir -p $SDK/{lib,include,bin}
#
# AWS SDK for C++: (requires libcurl, one of:
#                       libcurl4-openssl-dev
#                       libcurl4-nss-dev
#                       libcurl4-gnutls-dev)

#   cd /tmp && git clone --recurse-submodules https://github.com/aws/aws-sdk-cpp
#   cd aws-sdk-cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$SDK -DBUILD_ONLY="s3" -DBUILD_SHARED_LIBS=OFF -DENABLE_TESTING=OFF
#   cmake --build build -j$(nproc) && cmake --install build
#
# Google Cloud C++ Client (requires abseil, nlohmann/json, googletest, crc32c):
#   # Install abseil
#   cd /tmp && git clone https://github.com/abseil/abseil-cpp
#   cd abseil-cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$SDK -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DABSL_BUILD_TESTING=OFF -DABSL_PROPAGATE_CXX_STD=ON
#   cmake --build build -j$(nproc) && cmake --install build
#
#   # Install nlohmann/json
#   cd /tmp && git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git nlohmann_json
#   cd nlohmann_json && cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$SDK -DJSON_BuildTests=OFF
#   cmake --install build

#   # Install crc32c
#   cd /tmp && git clone https://github.com/google/crc32c.git
#   cd crc32c && mkdir build && cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$SDK -DCRC32C_USE_GLOG=NO -DCRC32C_BUILD_TESTS=NO -DCRC32C_BUILD_BENCHMARKS=NO
#   cmake --install build

#
#   # Install googletest
#   cd /tmp && git clone --depth 1 --branch v1.14.0 https://github.com/google/googletest.git
#   cd googletest && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$SDK
#   cmake --build build -j$(nproc) && cmake --install build
#
#   # Install Google Cloud C++
#   cd /tmp && git clone https://github.com/googleapis/google-cloud-cpp
#   cd google-cloud-cpp && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$SDK -DCMAKE_PREFIX_PATH="$SDK;$SDK/share/cmake/nlohmann_json" -DBUILD_TESTING=OFF -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF -DGOOGLE_CLOUD_CPP_ENABLE=storage
#   cmake --build build -j$(nproc) && cmake --install build
#
# Build sharq with cloud support:
#   cmake .. -DSHARQ_ENABLE_CLOUD=ON -DSHARQ_CLOUD_SDK_PREFIX=$HOME/local

