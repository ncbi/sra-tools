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

#
# Calculation of the global settings for the CMake build.
#

# allow implicit source file extensions
if ( ${CMAKE_VERSION} VERSION_EQUAL "3.20" OR
     ${CMAKE_VERSION} VERSION_GREATER "3.20")
    cmake_policy(SET CMP0115 OLD)
endif()

set( VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shared/toolkit.vers")
file( READ ${VERSION_FILE} VERSION )
string( STRIP ${VERSION} VERSION )
message( VERSION=${VERSION} )
string( REGEX MATCH "^[0-9]+" MAJVERS ${VERSION} )

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# determine OS
if ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Darwin" )
    set(OS "mac")
    set(SHLX "dylib")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Linux" )
    set(OS "linux")
    set(SHLX "so")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Windows" )
    set(OS "windows")
elseif()
    message ( FATAL_ERROR "unknown OS " ${CMAKE_HOST_SYSTEM_NAME})
endif()

# determine architecture
if ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "armv7l")
	set(ARCH "armv7l")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(ARCH "aarch64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(ARCH "x86_64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(ARCH "x86_64")
elseif()
    message ( FATAL_ERROR "unknown architecture " ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif ()

# create variables based entirely upon OS
if ( "mac" STREQUAL ${OS} )
    add_compile_definitions( MAC BSD UNIX )
elseif( "linux" STREQUAL ${OS} )
    add_compile_definitions( LINUX UNIX )
    set( LMCHECK -lmcheck )
elseif( "windows" STREQUAL ${OS} )
    add_compile_definitions( WINDOWS _WIN32_WINNT=0x0502 )
endif()

# create variables based entirely upon ARCH
# TODO: check if we need anything besides BITS in the if-else below
if ("armv7l" STREQUAL ${ARCH})
	set( BITS 32 )
    add_compile_definitions( RPI )
	add_compile_options( -mcpu=cortex-a53 -mfpu=neon-vfpv4 -Wno-psabi )
	set ( Z128SRC z128 )
	set ( Z128LSRC z128.nopt )
elseif ("aarch64" STREQUAL ${ARCH} )
	set ( BITS 64 )
	add_compile_definitions( HAVE_Z128 )
elseif ("x86_64" STREQUAL ${ARCH} )
    set ( BITS 64 )
    add_compile_definitions( LONG_DOUBLE_IS_NOT_R128 )
    if( NOT WIN32 )
        add_compile_definitions( HAVE_Z128 )
    endif()
endif()

# now any unique combinations of OS and ARCH
# TODO: check if this is needed
if     ( "mac-x86_84" STREQUAL ${OS}-${ARCH})
elseif ( "linux-x86_64" STREQUAL ${OS}-${ARCH})
    add_compile_definitions( HAVE_R128 )
elseif ( "linux-armv7l" STREQUAL ${OS}-${ARCH})
elseif ( "linux-aarch64" STREQUAL ${OS}-${ARCH})
    add_compile_definitions( HAVE_R128 __float128=_Float128 )
endif()

# combinations of OS and COMP
if ( ${OS}-${CMAKE_CXX_COMPILER_ID} STREQUAL "linux-GNU"  )
    add_definitions( -rdynamic )
    add_compile_definitions( _GNU_SOURCE )
endif()

# combinations of OS, ARCH and COMP
if ( ${OS}-${ARCH}-${CMAKE_CXX_COMPILER_ID} STREQUAL "linux-x86_64-GNU"  )
    add_compile_definitions( HAVE_QUADMATH )
	set ( LQUADMATH -lquadmath )
elseif ( ${OS}-${ARCH}-${CMAKE_CXX_COMPILER_ID} STREQUAL "linux-x86_64-Clang"  )
endif()

add_compile_definitions( _ARCH_BITS=${BITS} ${ARCH} ) # TODO ARCH ?
add_definitions( -Wall )

# assume debug build by default
if ( "${CMAKE_BUILD_TYPE}" STREQUAL "" )
    set ( CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE )
endif()

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
    add_compile_definitions( DEBUG _DEBUGGING )
else()
    add_compile_definitions( NDEBUG )
endif()

if ( SINGLE_TARGET )
    message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
endif()

message( "OS=" ${OS} " ARCH=" ${ARCH} " CXX=" ${CMAKE_CXX_COMPILER} " LMCHECK=" ${LMCHECK} " BITS=" ${BITS} " CMAKE_C_COMPILER_ID=" ${CMAKE_C_COMPILER_ID} " CMAKE_CXX_COMPILER_ID=" ${CMAKE_CXX_COMPILER_ID} )

# include directories for C/C++ compilation
#

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces ) # TODO: introduce a variable pointing to interfaces

#include_directories(interfaces/os)
#include_directories(interfaces/ext)

if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/gcc)
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/gcc/${ARCH})
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" )
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/clang)
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/clang/${ARCH})
elseif ( "MSVC" STREQUAL "${CMAKE_C_COMPILER_ID}")
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/vc++)
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/cc/vc++/${ARCH})
endif()

if ( "mac" STREQUAL ${OS} )
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/os/mac)
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/os/unix)
elseif( "linux" STREQUAL ${OS} )
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/os/linux)
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/os/unix)
elseif( "windows" STREQUAL ${OS} )
    include_directories(${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/os/win)
endif()

# TODO: should not be needed
if( NGS_INCDIR )
    include_directories( ${NGS_INCDIR} )
else ()
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/ngs/ngs-sdk )
    #TODO: if not found, checkout and build
endif ()

# TODO: properly initialize those directories somewhere on the top level
if( VDB_LIBDIR )
	message( "VDB_LIBDIR: ${VDB_LIBDIR}" )
	set( NCBI_VDB_LIBDIR ${VDB_LIBDIR} )
	set( NCBI_VDB_ILIBDIR ${VDB_LIBDIR}/../ilib )
else()
	set( NCBI_VDB_LIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/lib" )
	set( NCBI_VDB_ILIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/ilib" )
endif()
# set( NGS_LIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/lib" )
# set( NGS_ILIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/ilib" )
link_directories( ${NCBI_VDB_LIBDIR} )
link_directories( ${NCBI_VDB_ILIBDIR} )
# link_directories( ${NGS_LIBDIR} )
# link_directories( ${NGS_ILIBDIR} )

#/////////////////////// Cache variables, may be overridden at config time:

# by default, look for sister repositories sources side by side with ngs-tools, binaries under $OUTDIR if set, otherwise $HOME/ncbi-outdir
# if (NOT DEFINED OUTDIR)
    # if (DEFINED ENV{OUTDIR})
        # set ( OUTDIR "$ENV{OUTDIR}" )
    # else ()
        # set ( OUTDIR "$ENV{HOME}/ncbi-outdir" )
    # endif ()
# endif()

# set ( SRATOOLS_OUTDIR ${OUTDIR}/sra-tools/${OS}/${COMPILER}/${PLATFORM}/${BUILD} CACHE PATH "sra-tools output directory" )

# if (UNIX)
# #    set ( PLATFORM x86_64 )
    # if ( "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin" )
        # set ( OS mac )
        # set ( COMPILER clang )
    # else ()
        # set ( OS linux )
        # set ( COMPILER gcc )
    # endif()

    # # gmake is a single-configuration generator; we are either Debug or Release
    # if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        # set ( BUILD dbg )
    # else ()
        # set ( BUILD rel )
    # endif ()

    # set ( NGS_LIBDIR  ${OUTDIR}/sra-tools/${OS}/${COMPILER}/${PLATFORM}/${BUILD}/lib          CACHE PATH "sra-tools ngs library directory" )
    # set ( NGS_JAVADIR  ${OUTDIR}/sra-tools/ngs-java/                                                  CACHE PATH "ngs Java directory" )
    # set ( VDB_INCDIR  ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/                           CACHE PATH "ncbi-vdb include directory" )
    # set ( VDB_LIBDIR  ${OUTDIR}/ncbi-vdb/${OS}/${COMPILER}/${PLATFORM}/${BUILD}/lib         CACHE PATH "ncbi-vdb library directory" )
    # set ( VDB_ILIBDIR ${OUTDIR}/ncbi-vdb/${OS}/${COMPILER}/${PLATFORM}/${BUILD}/ilib        CACHE PATH "ncbi-vdb internal library directory" )
    # set ( SRATOOLS_BINDIR ${OUTDIR}/sra-tools/${OS}/${COMPILER}/${PLATFORM}/${BUILD}/bin    CACHE PATH "sra-tools executables directory" )
    # set ( NGSTOOLS_OUTDIR ${OUTDIR}/ngs-tools/${OS}/${COMPILER}/${PLATFORM}/${BUILD}        CACHE PATH "ngs-tools output directory" )

# elseif (WIN32)

    # set ( PLATFORM "x64" CACHE STRING "Windows Platform (x64 or Win32)" )
    # set ( OS win )
    # set ( COMPILER "vc++" )
    # if ( CMAKE_GENERATOR MATCHES "2010" )
        # set ( PLATFORM_TOOLSET "v100" )
    # elseif (CMAKE_GENERATOR MATCHES "2013" )
        # set ( PLATFORM_TOOLSET "v120" )
    # elseif (CMAKE_GENERATOR MATCHES "2017" )
        # set ( PLATFORM_TOOLSET "v141" )
    # else()
        # message( FATAL_ERROR "Unsupported generator for Windows: ${CMAKE_GENERATOR}." )
    # endif()

    # # by default, look for sister repositories sources side by side with ngs-tools, binaries under ../OUTDIR
    # set ( NGS_INCDIR  ${CMAKE_SOURCE_DIR}/../ngs/ngs-sdk/                                                   CACHE PATH "ngs include directory" )
    # set ( NGS_LIBDIR  ${OUTDIR}/ngs-sdk/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/$(Configuration)/lib          CACHE PATH "ngs library directory" )
    # set ( NGS_JAVADIR  ${OUTDIR}/ngs-java/                                                                  CACHE PATH "ngs Java directory" )
    # set ( VDB_INCDIR  ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/                                           CACHE PATH "ncbi-vdb include directory" )
    # set ( VDB_LIBDIR  ${OUTDIR}/ncbi-vdb/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/$(Configuration)/lib         CACHE PATH "ncbi-vdb library directory" )
    # set ( VDB_ILIBDIR ${OUTDIR}/ncbi-vdb/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/$(Configuration)/ilib        CACHE PATH "ncbi-vdb internal library directory" )
    # set ( SRATOOLS_BINDIR ${OUTDIR}/sra-tools/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/$(Configuration)/bin    CACHE PATH "sra-tools executables directory" )
    # set ( NGSTOOLS_OUTDIR ${OUTDIR}/ngs-tools/${OS}/${COMPILER}/${PLATFORM}/${BUILD}                        CACHE PATH "ngs-tools output directory")

# endif()

# #/////////////////////////////////////////////////////////////////////////////////////////////

# if (UNIX)

    # # default executables and libaries output directories
    # if ( NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY )
        # set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NGSTOOLS_OUTDIR}/bin )
    # endif()
    # if ( NOT DEFINED  CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
        # set ( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NGSTOOLS_OUTDIR}/ilib )
    # endif()

    # if ( "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin" )
        # set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.10 -stdlib=libc++" )
        # # on Mac, we may need some gcc headers in addition to clang's
        # include_directories ("${VDB_INCDIR}/cc/gcc/${PLATFORM}")
        # include_directories ("${VDB_INCDIR}/cc/gcc")
    # endif()

    # include_directories ("${VDB_INCDIR}/os/unix")

    # set ( SYS_LIBRARIES
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-ngs-c++${CMAKE_STATIC_LIBRARY_SUFFIX}
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-ngs-static${CMAKE_STATIC_LIBRARY_SUFFIX}
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-vdb-static${CMAKE_STATIC_LIBRARY_SUFFIX}
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ngs-c++${CMAKE_STATIC_LIBRARY_SUFFIX}
            # pthread
            # dl
    # )

    # set ( SYS_WLIBRARIES
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-ngs-static${CMAKE_STATIC_LIBRARY_SUFFIX}
            # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-wvdb-static${CMAKE_STATIC_LIBRARY_SUFFIX}
            # pthread
            # dl
    # )

    # if ( NOT DEFINED CMAKE_INSTALL_PREFIX)
        # set ( CMAKE_INSTALL_PREFIX /usr/local/ )
    # endif ()

    # set ( CPACK_GENERATOR "RPM;DEB;TGZ;" )

# elseif (WIN32)
    # if ( NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY )
        # set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM} )
    # endif()
    # if ( NOT DEFINED  CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
        # set ( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM} )
    # endif()
    # # Work configuration name into the NGSTOOLS_OUTDIR path; we do not want CMake to add /Debug|/Release at the end
    # SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/Debug/bin)
    # SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/Release/bin)
    # SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/Debug/ilib)
    # SET( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${NGSTOOLS_OUTDIR}/${OS}/${PLATFORM_TOOLSET}/${PLATFORM}/Release/ilib)

    # include_directories ("${NGS_INCDIR}/win")

    # # use miltiple processors
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    # # use Unicode
    # add_definitions(-DUNICODE -D_UNICODE)

    # # static run time libraries
    # set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT" )
    # set ( CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   /MTd" )
    # set ( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT" )
    # set ( CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   /MTd" )

    # string(REPLACE "$(Configuration)" "Debug"   NGS_LIBDIR_DEBUG ${NGS_LIBDIR})
    # string(REPLACE "$(Configuration)" "Release" NGS_LIBDIR_RELEASE ${NGS_LIBDIR})
    # string(REPLACE "$(Configuration)" "Debug"   VDB_LIBDIR_DEBUG ${VDB_LIBDIR})
    # string(REPLACE "$(Configuration)" "Release" VDB_LIBDIR_RELEASE ${VDB_LIBDIR})

    # set ( SYS_LIBRARIES
        # ${CMAKE_STATIC_LIBRARY_PREFIX}bz2${CMAKE_STATIC_LIBRARY_SUFFIX}
        # ${CMAKE_STATIC_LIBRARY_PREFIX}zlib${CMAKE_STATIC_LIBRARY_SUFFIX}
        # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-vdb${CMAKE_STATIC_LIBRARY_SUFFIX}
        # ${CMAKE_STATIC_LIBRARY_PREFIX}ncbi-ngs${CMAKE_STATIC_LIBRARY_SUFFIX}
        # libngs-bind-c++${CMAKE_STATIC_LIBRARY_SUFFIX}
        # libngs-disp${CMAKE_STATIC_LIBRARY_SUFFIX}
        # ws2_32
        # Crypt32
    # )

    # set ( CPACK_GENERATOR "ZIP" )

# else()
    # message ( FATAL_ERROR "Unsupported OS" )
# endif()

# # Java needs
# find_package(Java REQUIRED)
# find_program(ANT_EXECUTABLE ant PATHS $ENV{ANT_HOME} ENV PATH )
# if ( NOT ANT_EXECUTABLE )
    # message ( WARNING "Failed to locate 'ant' executable in PATH or ANT_HOME. Please specify path to 'ant' via ANT_EXECUTABLE" )
# endif()
# if ( NOT DEFINED ENV{JAVA_HOME} )
    # message ( STATUS "Warning: JAVA_HOME is not set, 'ant' scripts may work incorrectly" )
# endif ()
# set ( NGSJAR "${NGS_JAVADIR}/jar/ngs-java.jar" )
# set ( CMAKE_JAVA_COMPILE_FLAGS "-Xmaxerrs" "1" )

# # look for dependencies

# if (NOT EXISTS ${NGS_INCDIR})
    # message( FATAL_ERROR "NGS includes are not found in ${NGS_INCDIR}." )
# else()
    # message( STATUS "Found NGS includes in ${NGS_INCDIR}. Looking for NGS libraries..." )

    # if (UNIX)
        # find_library ( NGS_LIBRARY ngs-c++ PATHS ${NGS_LIBDIR} NO_DEFAULT_PATH )
		# if ( NGS_LIBRARY )
			# get_filename_component(NGS_LIBRARY_DIR ${NGS_LIBRARY} PATH)
			# message ( STATUS "Found NGS libraries in ${NGS_LIBDIR}" )
		# else ()
			# message( FATAL_ERROR "NGS libraries are not found in ${NGS_LIBDIR}." )
		# endif()
    # else()

		# # on Windows, require both debug and release libraries
        # if (CMAKE_CONFIGURATION_TYPES MATCHES ".*Debug.*")
            # find_library ( NGS_LIBRARY libngs-bind-c++ PATHS ${NGS_LIBDIR_DEBUG} NO_DEFAULT_PATH )
			# if ( NGS_LIBRARY )
				# get_filename_component(NGS_LIBRARY_DIR ${NGS_LIBRARY} PATH)
				# message ( STATUS "Found Debug NGS libraries in ${NGS_LIBDIR_DEBUG}" )
			# else ()
				# message( FATAL_ERROR "NGS libraries are not found in ${NGS_LIBDIR_DEBUG}." )
			# endif()
        # endif()

        # if (CMAKE_CONFIGURATION_TYPES MATCHES ".*Release.*")
            # find_library ( NGS_LIBRARY libngs-bind-c++ PATHS ${NGS_LIBDIR_RELEASE} NO_DEFAULT_PATH )
			# if ( NGS_LIBRARY )
				# get_filename_component(NGS_LIBRARY_DIR ${NGS_LIBRARY} PATH)
				# message ( STATUS "Found Release NGS libraries in ${NGS_LIBRARY_DIR}" )
			# else ()
				# message( FATAL_ERROR "NGS libraries are not found in ${NGS_LIBDIR_RELEASE}." )
			# endif()
        # endif()

    # endif()

    # unset ( NGS_LIBRARY )
# endif()

# if (NOT EXISTS ${VDB_INCDIR})
    # message( FATAL_ERROR "NCBI-VDB includes are not found in ${VDB_INCDIR}" )
# else ()
    # message( STATUS "Found NCBI-VDB includes in ${VDB_INCDIR}. Looking for NCBI-VDB libraries..." )
    # if (UNIX)
        # find_library ( VDB_LIBRARY ncbi-vdb PATHS ${VDB_LIBDIR} NO_DEFAULT_PATH )
    # else()

		# # on Windows, require both debug and release libraries
        # if (CMAKE_CONFIGURATION_TYPES MATCHES ".*Debug.*")
            # find_library ( VDB_LIBRARY ncbi-vdb PATHS ${VDB_LIBDIR_DEBUG} NO_DEFAULT_PATH )
			# if ( VDB_LIBRARY )
				# get_filename_component(VDB_LIBRARY ${VDB_LIBRARY} PATH)
				# message ( STATUS "Found Debug NCBI-VDB libraries in ${VDB_LIBRARY}" )
			# else ()
				# message( FATAL_ERROR "NCBI-VDB libraries are not found in ${VDB_LIBDIR_DEBUG}." )
			# endif()
        # endif()

        # if (CMAKE_CONFIGURATION_TYPES MATCHES ".*Release.*")
            # find_library ( VDB_LIBRARY ncbi-vdb PATHS ${VDB_LIBDIR_RELEASE} NO_DEFAULT_PATH )
			# if ( VDB_LIBRARY )
				# get_filename_component(VDB_LIBRARY ${VDB_LIBRARY} PATH)
				# message ( STATUS "Found Release NCBI-VDB libraries in ${VDB_LIBDIR_RELEASE}" )
			# else ()
				# message( FATAL_ERROR "NCBI-VDB libraries are not found in ${VDB_LIBDIR_RELEASE}." )
			# endif()
        # endif()

    # endif()

    # unset ( VDB_LIBRARY )
# endif()

# #//////////////////////////////////////////////////

# include_directories ("${VDB_INCDIR}")
# include_directories ("${VDB_INCDIR}/cc/${COMPILER}/${PLATFORM}")
# include_directories ("${VDB_INCDIR}/cc/${COMPILER}")
# include_directories ("${VDB_INCDIR}/os/${OS}")
# include_directories ("${NGS_INCDIR}")

# link_directories (  ${VDB_ILIBDIR} ${VDB_LIBDIR} ${NGS_LIBDIR} )

# #/////////////////////////////////////////////////
# # versioned names, symbolic links and installation for the tools

# function ( links_and_install_subdir TARGET INST_SUBDIR)

    # if (WIN32)

        # install ( TARGETS ${TARGET} RUNTIME DESTINATION bin )

    # else()

        # # on Unix, version the binary and add 2 symbolic links
        # set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${TARGET}.${VERSION}")

        # set (TGTPATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET})

        # ADD_CUSTOM_TARGET(link_${TARGET} ALL
                        # COMMAND ${CMAKE_COMMAND}   -E create_symlink ${TARGET}.${VERSION} ${TGTPATH}.${MAJVERS}
                        # COMMAND ${CMAKE_COMMAND}   -E create_symlink ${TARGET}.${MAJVERS} ${TGTPATH}
                        # DEPENDS ${TARGET}
                        # )

        # install ( TARGETS ${TARGET} RUNTIME     DESTINATION bin/${INST_SUBDIR} )
        # install ( FILES ${TGTPATH}.${MAJVERS}   DESTINATION bin/${INST_SUBDIR} )
        # install ( FILES ${TGTPATH}              DESTINATION bin/${INST_SUBDIR} )

    # endif()

# endfunction(links_and_install_subdir)

# function ( links_and_install TARGET )
    # links_and_install_subdir( ${TARGET} "" )
# endfunction(links_and_install)

enable_testing()

function(SetAndCreate var path )
    if( NOT DEFINED ${var} )
        set( ${var} ${path} PARENT_SCOPE )
    endif()
    file(MAKE_DIRECTORY ${path} )
endfunction()

if ( ${CMAKE_GENERATOR} MATCHES "Visual Studio.*" OR
     ${CMAKE_GENERATOR} STREQUAL "Xcode" )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/Debug/ilib )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/ilib )
    SetAndCreate( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/Debug/lib )
    SetAndCreate( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib )
    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/Debug/bin )
    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin )

    set( SINGLE_CONFIG false )
else() # assume a single-config generator
    SetAndCreate( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
    set( SINGLE_CONFIG true )
endif()


function( MSVS_StaticRuntime name )
    if( WIN32 )
        set_property(TARGET ${name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endfunction()

function( MSVS_DLLRuntime name )
    if( WIN32 )
        set_property(TARGET ${name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endfunction()

function( GenerateStaticLibsWithDefs target_name sources compile_defs )
    add_library( ${target_name} STATIC ${sources} )
    if( NOT "" STREQUAL "${compile_defs}" )
        target_compile_definitions( ${target_name} PRIVATE ${compile_defs} )
    endif()
    if( WIN32 )
        MSVS_StaticRuntime( ${target_name} )
        add_library( ${target_name}-md STATIC ${sources} )
        if(NOT "" STREQUAL "${compile_defs}" )
            target_compile_definitions( ${target_name}-md PRIVATE ${compile_defs} )
        endif()
        MSVS_DLLRuntime( ${target_name}-md )
    endif()
endfunction()

function( GenerateStaticLibs target_name sources )
   GenerateStaticLibsWithDefs( ${target_name} "${sources}" "" )
endfunction()

function( ExportStatic name install )
    # the output goes to .../lib
    if( SINGLE_CONFIG )
        # make the output name versioned, create all symlinks
        set_target_properties( ${name} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
        add_custom_command(TARGET ${name}
            POST_BUILD
            COMMAND rm -f lib${name}.a.${VERSION}
            COMMAND mv lib${name}.a lib${name}.a.${VERSION}
            COMMAND ln -f -s lib${name}.a.${VERSION} lib${name}.a.${MAJVERS}
            COMMAND ln -f -s lib${name}.a.${MAJVERS} lib${name}.a
            COMMAND ln -f -s lib${name}.a lib${name}-static.a
            WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        )
        if ( ${install} )
            install( FILES  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a.${VERSION}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a.${MAJVERS}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}-static.a
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64
            )
         endif()
    else()
        set_target_properties( ${name} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG})
        set_target_properties( ${name} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE})
    endif()
endfunction()


#
# create versioned names and symlinks for a shared library
#
function(MakeLinksShared target name install)
    if( SINGLE_CONFIG )
        add_custom_command(TARGET ${target}
            POST_BUILD
            COMMAND rm -f lib${name}.${SHLX}.${VERSION}
            COMMAND mv lib${name}.${SHLX} lib${name}.${SHLX}.${VERSION}
            COMMAND ln -f -s lib${name}.${SHLX}.${VERSION} lib${name}.${SHLX}.${MAJVERS}
            COMMAND ln -f -s lib${name}.${SHLX}.${MAJVERS} lib${name}.${SHLX}
            WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        )
        if ( ${install} )
            install( FILES  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${VERSION}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${MAJVERS}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64
        )
        endif()
    endif()
endfunction()

#
# for a static library target, create a public shared target with the same base name and contents
#
function(ExportShared lib install)
    get_target_property( src ${lib} SOURCES )
    add_library( ${lib}-shared SHARED ${src} )
    MSVS_StaticRuntime( ${lib}-shared )
    set_target_properties( ${lib}-shared PROPERTIES OUTPUT_NAME ${lib} )
    MakeLinksShared( ${lib}-shared ${lib} ${install} )

    if( WIN32 )
        add_library( ${lib}-shared-md SHARED ${src} )
        MSVS_DLLRuntime( ${lib}-shared-md )
        set_target_properties( ${lib}-shared-md PROPERTIES OUTPUT_NAME ${lib}-md )
    endif()
endfunction()

set( CMAKE_POSITION_INDEPENDENT_CODE True )

message( "CMAKE_LIBRARY_OUTPUT_DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" )

set( COMMON_LINK_LIBRARIES kapp tk-version )
if( WIN32 )
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
    set( COMMON_LINK_LIBRARIES  ${COMMON_LINK_LIBRARIES} Ws2_32 Crypt32 )
else()
    # link_libraries( ${COMMON_LINK_LIBRARIES} )
    # set( COMMON_LIBS_READ   ncbi-vdb  dl pthread )
    # set( COMMON_LIBS_WRITE  ncbi-wvdb dl pthread )
endif()

