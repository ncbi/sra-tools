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

if( SINGLE_CONFIG )
    
    cmake_minimum_required(VERSION 3.16)
    project(sharq)
    
    # prefer static linkage
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .so .dylib .tbd)
    find_package(ZLIB)
    find_package(BZip2)
    find_package (Threads)
    
    # check whether RE2 is accessible
    set(RE2_URL https://github.com/google/re2)
    execute_process(COMMAND curl -Is ${RE2_URL}
                    COMMAND head -n 1
                    COMMAND grep -q 200
                    RESULT_VARIABLE RE2_RC)
    if ("${RE2_RC}" EQUAL 0)
      set( RE2_FOUND TRUE CACHE INTERNAL "" )
    else()
      message(RE2_RC = "${RE2_RC}" )
      set( RE2_FOUND FALSE CACHE INTERNAL "" )
    endif()
    message(RE2_FOUND = ${RE2_FOUND} )
    # RE2_FOUND will be used by sharQ tests
    
    if ( ZLIB_FOUND AND BZip2_FOUND AND Threads_FOUND AND RE2_FOUND )
        #set(CMAKE_VERBOSE_MAKEFILE ON)
        set(CMAKE_CXX_STANDARD 17)
        
        #specific to BitMagic
        if (COMPILER_OPTION_SSE42_SUPPORTED)
            add_definitions(-msse4.2 -DBMSSE42OPT)
        endif()
        
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
        set(LOCAL_BUILD_DIR ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
        set(LOCAL_INCDIR ${LOCAL_BUILD_DIR}/include)
        set(LOCAL_LIBDIR ${LOCAL_BUILD_DIR}/lib)
        
        # Google RE2 release
        set(RE2_TAG 2021-09-01)
        #set(RE2_TAG 2023-03-01)
        
        include (ExternalProject)
        ExternalProject_Add(RE2
          PREFIX ${LOCAL_BUILD_DIR}
          GIT_REPOSITORY ${RE2_URL}
          GIT_TAG ${RE2_TAG}
          BUILD_IN_SOURCE 1
          UPDATE_COMMAND "" # No update as we use specific tag
          INSTALL_DIR ${LOCAL_LIBDIR}
          CMAKE_ARGS
            -DCMAKE_BUILD_TYPE:STRING=Release
            -DRE2_BUILD_TESTING:BOOL=OFF
            -DCMAKE_INSTALL_PREFIX:PATH=${LOCAL_BUILD_DIR}
            -DCMAKE_INSTALL_LIBDIR:PATH=lib
            -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
            -DCMAKE_INSTALL_MESSAGE:STRING=LAZY
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        )
        set(RE2_STATIC_LIBRARIES -L${LOCAL_LIBDIR} -lre2 )
        
        add_custom_target(
          sharq.py
          DEPENDS ${CMAKE_SOURCE_DIR}/tools/loaders/sharq/sharq.py
          COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/tools/loaders/sharq/sharq.py
            ${BINDIR}/sharq.py)
        
        add_executable(sharq fastq_parse.cpp)
        add_dependencies(sharq RE2 sharq.py)
        target_include_directories(sharq PUBLIC ${LOCAL_INCDIR} ${CMAKE_SOURCE_DIR}/libs/inc ./ ../../../)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
        endif()
        target_link_libraries(sharq ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${COMMON_LIBS_READ})
        MakeLinksExe( sharq false )
        
#        if( RUN_SANITIZER_TESTS )
#            set( asan_defs "-fsanitize=address" )
#            add_executable(sharq-asan fastq_parse.cpp )
#            add_dependencies(sharq-asan RE2 sharq.py)
#            target_include_directories(sharq-asan PUBLIC ${LOCAL_INCDIR} ${CMAKE_SOURCE_DIR}/libs/inc ./ ../../../)
#            target_link_libraries(sharq-asan ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
#            target_compile_options( sharq-asan PRIVATE ${asan_defs} )
#            target_link_options( sharq-asan PRIVATE ${asan_defs} )
#        
#            set( tsan_defs "-fsanitize=thread" )
#            add_executable(sharq-tsan fastq_parse.cpp )
#            add_dependencies(sharq-tsan RE2 sharq.py)
#            target_include_directories(sharq-tsan PUBLIC ${LOCAL_INCDIR} ${CMAKE_SOURCE_DIR}/libs/inc ./ ../../../)
#            target_link_libraries(sharq-tsan ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
#            target_compile_options( sharq-tsan PRIVATE ${tsan_defs} )
#            target_link_options( sharq-tsan PRIVATE ${tsan_defs} )
#        endif()
    
    else()
      message( "SharQ is not built:" )
      message( ZLIB_FOUND=${ZLIB_FOUND} )
      message( BZip2_FOUND=${BZip2_FOUND} )
      message( Threads_FOUND=${Threads_FOUND} )
      message( RE2_FOUND=${RE2_FOUND} )
    endif()

endif()
