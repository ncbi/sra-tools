
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
#
# This is the CMake build for sharq project.
#
# Usage
#   cmake [options] <path-to-source>
#
# Parameter:
#   path-to-source    : path to this file's directory
#
#   In addition, you can use all the standard CMake options, e.g.:
#   -DCMAKE_BUILD_TYPE=<Debug|Release>
#
## ===========================================================================

if( NOT WIN32 )

cmake_minimum_required(VERSION 3.16)
project(sharq)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

#specific to BitMagic
add_definitions(-msse4.2 -DBMSSE42OPT)

find_package(ZLIB)
find_package(BZip2)
#find_package(LZMA)
find_package (Threads)

set(LOCAL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs)
set(LOCAL_INCDIR ${LOCAL_BUILD_DIR}/include)
set(LOCAL_LIBDIR ${LOCAL_BUILD_DIR}/lib)

# Google RE2 release
set(RE2_TAG 2021-09-01)

include (ExternalProject)
ExternalProject_Add(RE2      
  PREFIX RE2      
  GIT_REPOSITORY https://github.com/google/re2.git
  GIT_TAG ${RE2_TAG}    
  BUILD_IN_SOURCE 1
  UPDATE_COMMAND "" # No update as we use specific tag
  #INSTALL_DIR ${LOCAL_BUILD_DIR}
  CMAKE_ARGS    
    -DCMAKE_BUILD_TYPE:STRING=Release 
    -DRE2_BUILD_TESTING:BOOL=OFF 
    -DCMAKE_INSTALL_PREFIX:PATH=${LOCAL_BUILD_DIR} 
    -DCMAKE_INSTALL_LIBDIR:PATH=lib
    -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF 
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON  
    -DCMAKE_INSTALL_MESSAGE:STRING=LAZY
    
)
set(RE2_STATIC_LIBRARIES -L${LOCAL_LIBDIR} -lre2 )

add_executable(sharq fastq_parse )
add_dependencies(sharq RE2)
target_include_directories(sharq PUBLIC ${LOCAL_INCDIR} ./)
target_link_libraries(sharq stdc++fs ZLIB::ZLIB ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

endif()