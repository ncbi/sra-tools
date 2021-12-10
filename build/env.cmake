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
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Linux" )
    set(OS "linux")
    set(SHLX "so")
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Windows" )
    set(OS "windows")
    set(STLX "lib")
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
    set( LMCHECK "" )
    set( EXE "" )
elseif( "linux" STREQUAL ${OS} )
    add_compile_definitions( LINUX UNIX )
    set( LMCHECK -lmcheck )
    set( EXE "" )
elseif( "windows" STREQUAL ${OS} )
    add_compile_definitions( WINDOWS _WIN32_WINNT=0x0502 )
    set( LMCHECK "" )
    set( EXE ".exe" )
endif()

# create variables based entirely upon ARCH
# TODO: check if we need anything besides BITS in the if-else below
if ("armv7l" STREQUAL ${ARCH})
	set( BITS 32 )
	add_compile_options( -Wno-psabi )
elseif ("aarch64" STREQUAL ${ARCH} )
	set ( BITS 64 )
elseif ("x86_64" STREQUAL ${ARCH} )
    set ( BITS 64 )
endif()

# now any unique combinations of OS and ARCH
# TODO: check if this is needed
if     ( "mac-x86_84" STREQUAL ${OS}-${ARCH})
elseif ( "linux-x86_64" STREQUAL ${OS}-${ARCH})
elseif ( "linux-armv7l" STREQUAL ${OS}-${ARCH})
elseif ( "linux-aarch64" STREQUAL ${OS}-${ARCH})
    add_compile_definitions( __float128=_Float128 )
endif()

# combinations of OS and COMP
if ( ${OS}-${CMAKE_CXX_COMPILER_ID} STREQUAL "linux-GNU"  )
    add_definitions( -rdynamic )
    add_compile_definitions( _GNU_SOURCE )
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

# ===========================================================================
# 3d party packages

# Flex/Bison
find_package( FLEX 2.6 )
find_package( BISON 3 )

find_package( LibXml2 )

find_package(Java COMPONENTS Development)
if( Java_FOUND AND NOT Java_VERSION )
    message(STATUS "No version of Java found")
    unset( Java_FOUND )
endif()

if ( PYTHON_PATH )
    set( Python3_EXECUTABLE ${PYTHON_PATH} )
endif()
find_package( Python3 COMPONENTS Interpreter )

# ===========================================================================

enable_testing()

# ===========================================================================
# singfle vs. multitarget generators, ncbi-vdb binaries

function(SetAndCreate var path )
    if( NOT DEFINED ${var} )
        set( ${var} ${path} PARENT_SCOPE )
    endif()
    file(MAKE_DIRECTORY ${path} )
endfunction()

if( NOT TARGDIR )
    set( TARGDIR ${CMAKE_BINARY_DIR} )
endif()

if ( ${CMAKE_GENERATOR} MATCHES "Visual Studio.*" OR
     ${CMAKE_GENERATOR} STREQUAL "Xcode" )
    set( SINGLE_CONFIG false )

    if( NOT VDB_BINDIR OR NOT EXISTS ${VDB_BINDIR} )
        message( FATAL_ERROR "Please specify the location of an ncbi-vdb build in Cmake variable VDB_BINDIR. It is expected to contain one or both subdirectories Debug/ and Release/, with bin/, lib/ and ilib/ underneath each.")
    endif()

    set( NCBI_VDB_BINDIR_DEBUG ${VDB_BINDIR}/Debug/bin )
    set( NCBI_VDB_BINDIR_RELEASE ${VDB_BINDIR}/Release/bin )

    set( NCBI_VDB_LIBDIR_DEBUG ${VDB_BINDIR}/Debug/lib )
    set( NCBI_VDB_LIBDIR_RELEASE ${VDB_BINDIR}/Release/lib )

    set( NCBI_VDB_ILIBDIR_DEBUG ${VDB_BINDIR}/Debug/ilib )
    set( NCBI_VDB_ILIBDIR_RELEASE ${VDB_BINDIR}/Release/ilib )

    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${TARGDIR}/Debug/bin )
    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TARGDIR}/Release/bin )
    SetAndCreate( TEST_RUNTIME_OUTPUT_DIRECTORY_DEBUG    ${TARGDIR}/Debug/test-bin )
    SetAndCreate( TEST_RUNTIME_OUTPUT_DIRECTORY_RELEASE  ${TARGDIR}/Release/test-bin )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   ${TARGDIR}/Debug/lib )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${TARGDIR}/Release/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG            ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG} )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE          ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE} )
    set( CMAKE_JAR_OUTPUT_DIRECTORY                      ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG} ) # both Debug and Release share this
    SetAndCreate( TEMPDIR_DEBUG "${TEST_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/tmp" )
    SetAndCreate( TEMPDIR_RELEASE "${TEST_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/tmp" )
    cmake_path(NATIVE_PATH TEMPDIR_DEBUG NORMALIZE TEMPDIR_DEBUG)
    cmake_path(NATIVE_PATH TEMPDIR_RELEASE NORMALIZE TEMPDIR_RELEASE)

    # to be used in add-test() as the location of executables, etc.
    # NOTE: always use the COMMAND_EXPAND_LISTS option of add_test
    set( BINDIR "$<$<CONFIG:Debug>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>" )
    set( LIBDIR "$<$<CONFIG:Debug>:${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE}>" )
    set( TESTBINDIR "$<$<CONFIG:Debug>:${TEST_RUNTIME_OUTPUT_DIRECTORY_DEBUG}>$<$<CONFIG:Release>:${TEST_RUNTIME_OUTPUT_DIRECTORY_RELEASE}>" )
    set( TEMPDIR "$<$<CONFIG:Debug>:${TEMPDIR_DEBUG}>$<$<CONFIG:Release>:${TEMPDIR_RELEASE}>" )

    link_directories( $<$<CONFIG:Debug>:${NCBI_VDB_LIBDIR_DEBUG}> $<$<CONFIG:Release>:${NCBI_VDB_LIBDIR_RELEASE}> )
    link_directories( $<$<CONFIG:Debug>:${NCBI_VDB_ILIBDIR_DEBUG}> $<$<CONFIG:Release>:${NCBI_VDB_ILIBDIR_RELEASE}> )

else() # assume a single-config generator
    set( SINGLE_CONFIG true )

    # if( NOT VDB_BINDIR OR NOT EXISTS ${VDB_BINDIR} )
        # message( FATAL_ERROR "Please specify the location of an ncbi-vdb build in Cmake variable VDB_BINDIR. It is expected to contain subdirectories bin/, lib/, ilib/.")
    # endif()

    if( NOT VDB_LIBDIR OR NOT EXISTS ${VDB_LIBDIR} )
        message( FATAL_ERROR "Please specify the location where ncbi-vdb libraries are installed (VDB_LIBDIR)")
    endif()

    #set( NCBI_VDB_BINDIR ${VDB_BINDIR}/bin )
    #set( NCBI_VDB_LIBDIR ${VDB_BINDIR}/lib )
    set( NCBI_VDB_LIBDIR ${VDB_LIBDIR} )
    #set( NCBI_VDB_ILIBDIR ${VDB_BINDIR}/ilib )
    message(WARNING "Linking with ncbi-vdb libraries from the following location: ${NCBI_VDB_LIBDIR}")

    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TARGDIR}/bin )
    SetAndCreate( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${TARGDIR}/lib )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${TARGDIR}/ilib )
    set( CMAKE_JAR_OUTPUT_DIRECTORY              ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )

    # to be used in add-test() as the location of executables.
    # NOTE: always use the COMMAND_EXPAND_LISTS option of add_test
    set( BINDIR "${TARGDIR}/bin" )
    set( LIBDIR "${TARGDIR}/lib" )
    set( TESTBINDIR "${TARGDIR}/test-bin" )
    SetAndCreate( TEMPDIR "${TESTBINDIR}/tmp" )

    link_directories( ${NCBI_VDB_LIBDIR} ) # TODO: USE_INSTALLED_NCBI_VDB
    #link_directories( ${NCBI_VDB_ILIBDIR} ) # TODO: not clear what to do in case USE_INSTALLED_NCBI_VDB == 1
endif()

# ===========================================================================
# ncbi-vdb sources

# Using installed ncbi-vdb directory
if( WIN32 )
	# TODO: WIN32 and Mac still work in an assumtion that ncbi-vdb sources is checked out along with sra-tools
	set( USE_INSTALLED_NCBI_VDB 0 )
elseif( SINGLE_CONFIG )
	set( USE_INSTALLED_NCBI_VDB 1 )
else() # XCode
	# TODO: WIN32 and Mac still work in an assumtion that ncbi-vdb sources is checked out along with sra-tools
	set( USE_INSTALLED_NCBI_VDB 0 )
endif()

if( NOT VDB_SRCDIR )
	if( USE_INSTALLED_NCBI_VDB )
		message("CMAKE_INSTALL_PREFIX_ROOT: ${CMAKE_INSTALL_PREFIX_ROOT}")
		set( NCBI_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX_ROOT} )

		set( VDB_SRCDIR "${NCBI_INSTALL_PREFIX}/ncbi-vdb/vdb_shared_sources" )
		set( VDB_INTERFACES_DIR "${NCBI_INSTALL_PREFIX}/ncbi-vdb/interfaces" )

		if ( NOT EXISTS ${VDB_SRCDIR} )
			message("${VDB_SRCDIR} does not exist - ncbi-vdb was not installed in that location, falling back to the standard ncbi-vdb build location ${CMAKE_SOURCE_DIR}/../ncbi-vdb")
			set( VDB_SRCDIR ${CMAKE_SOURCE_DIR}/../ncbi-vdb )
		else()
			message(WARNING "Using INSTALLED ncbi-vdb sources: ${VDB_SRCDIR}")
		endif()

		if ( NOT EXISTS ${VDB_INTERFACES_DIR} )
			message("${VDB_INTERFACES_DIR} does not exist - ncbi-vdb was not installed in that location, falling back to the standard ncbi-vdb build location VDB_INTERFACES_DIR ${VDB_SRCDIR}/interfaces...")
			set( VDB_INTERFACES_DIR ${VDB_SRCDIR}/interfaces )
		else()
			message(WARNING "Using INSTALLED ncbi-vdb interfaces: ${VDB_INTERFACES_DIR}")
		endif()
	else()
		set( VDB_SRCDIR ${CMAKE_SOURCE_DIR}/../ncbi-vdb )
		set( VDB_INTERFACES_DIR ${VDB_SRCDIR}/interfaces )

		message(WARNING "Using ncbi-vdb sources from: ${VDB_SRCDIR}, interfaces: ${VDB_INTERFACES_DIR}")
	endif()

	message("VDB_SRCDIR was not explicitly provided, using the following location: sources: ${VDB_SRCDIR}, interfaces: ${VDB_INTERFACES_DIR}")

	if ( NOT EXISTS ${VDB_SRCDIR} )
		message( FATAL_ERROR "${VDB_SRCDIR} does not exist. Please specify the location of ncbi-vdb sources in Cmake variable VDB_SRCDIR")
	endif()
endif()


include_directories( ${VDB_INTERFACES_DIR} )
include_directories( ${VDB_SRCDIR}/libs ) # /libs for ngs/ncbi/ngs/NGS_FragmentBlob.c:39:10

if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
    include_directories(${VDB_INTERFACES_DIR}/cc/gcc)
    include_directories(${VDB_INTERFACES_DIR}/cc/gcc/${ARCH})
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" )
    include_directories(${VDB_INTERFACES_DIR}/cc/clang)
    include_directories(${VDB_INTERFACES_DIR}/cc/clang/${ARCH})
elseif ( "MSVC" STREQUAL "${CMAKE_C_COMPILER_ID}")
    include_directories(${VDB_INTERFACES_DIR}/cc/vc++)
    include_directories(${VDB_INTERFACES_DIR}/cc/vc++/${ARCH})
endif()

if ( "mac" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/mac)
    include_directories(${VDB_INTERFACES_DIR}/os/unix)
elseif( "linux" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/linux)
    include_directories(${VDB_INTERFACES_DIR}/os/unix)
elseif( "windows" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/win)
endif()

include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

# ===========================================================================

# DIRTOTEST is the overridable location of the executables to call from scripted test
if( NOT DIRTOTEST )
    set( DIRTOTEST ${BINDIR} )
endif()
message( DIRTOTEST: ${DIRTOTEST})

# CONFIGTOUSE is a way to block user settings ($HOME/.ncbi/user-settings.mkfg). Assign anything but NCBI_SETTINGS to it, and the user settings will be ignored.
if( NOT CONFIGTOUSE )
    set( CONFIGTOUSE NCBI_SETTINGS )
endif()
message( CONFIGTOUSE: ${CONFIGTOUSE})

# Python 3
if( Python3_EXECUTABLE )
    set( PythonUserBase ${TEMPDIR}/python )
endif()

#
# Common functions for creation of build artefacts
#

function( GenerateStaticLibsWithDefs target_name sources compile_defs )
    add_library( ${target_name} STATIC ${sources} )
    if( NOT "" STREQUAL "${compile_defs}" )
        target_compile_definitions( ${target_name} PRIVATE ${compile_defs} )
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

        set_property(
            TARGET    ${name}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a.${VERSION};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a.${MAJVERS};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.a;${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}-static.a"
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
        if ( ${install} )
            install( TARGETS ${name} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )
        endif()
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

        set_property(
            TARGET    ${target}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${VERSION};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${MAJVERS};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}"
        )

        if ( ${install} )
            install( FILES  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${VERSION}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}.${MAJVERS}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib64
        )
        endif()
    else()
        set_target_properties( ${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
        set_target_properties( ${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
        if ( ${install} )
            install( TARGETS ${target}
                     ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                     RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )
        endif()
    endif()
endfunction()

#
# for a static library target, create a public shared target with the same base name and contents
#
function(ExportShared lib install)
    if ( NOT WIN32 )
        get_target_property( src ${lib} SOURCES )
        add_library( ${lib}-shared SHARED ${src} )
        set_target_properties( ${lib}-shared PROPERTIES OUTPUT_NAME ${lib} )
        MakeLinksShared( ${lib}-shared ${lib} ${install} )
    endif()
endfunction()

#
# create versioned names and symlinks for an executable
#
function(MakeLinksExe target install_via_driver)
    if( SINGLE_CONFIG )
        add_custom_command(TARGET ${target}
            POST_BUILD
            COMMAND rm -f ${target}.${VERSION}
            COMMAND mv ${target} ${target}.${VERSION}
            COMMAND ln -f -s ${target}.${VERSION} ${target}.${MAJVERS}
            COMMAND ln -f -s ${target}.${MAJVERS} ${target}
            COMMAND ln -f -s sratools.${VERSION} ${target}-driver
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )

        set_property(
            TARGET    ${target}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${VERSION};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${MAJVERS};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}-driver"
        )

        if ( install_via_driver )
            install(
                PROGRAMS
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${MAJVERS}
                DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )
            install(
                PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${VERSION}
                RENAME ${target}-orig.${VERSION}
                DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )
            install(
                PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}-driver
                RENAME ${target}.${VERSION}
                DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )

        else()
            install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${VERSION}
                              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${MAJVERS}
                              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )
        endif()
    else()
        if ( install_via_driver )
                install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}${EXE}
                         RENAME ${target}-orig${EXE}
                         DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                )
                install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sratools${EXE}
                         RENAME ${target}${EXE}
                         DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
                )
        else()
            install( TARGETS ${target} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )
        endif()

    endif()
endfunction()


if( WIN32 )
    set( COMMON_LINK_LIBRARIES kapp load tk-version )
    set( COMMON_LIBS_READ  ncbi-vdb.${STLX} )
    set( COMMON_LIBS_WRITE ncbi-wvdb.${STLX} )
else()
    # single-config generators need full path to ncbi-vdb libraries in order to handle the dependency correctly
    set( COMMON_LINK_LIBRARIES ${NCBI_VDB_ILIBDIR}/libkapp.${STLX} ${NCBI_VDB_ILIBDIR}/libload.${STLX} tk-version )
    set( COMMON_LIBS_READ   ${NCBI_VDB_LIBDIR}/libncbi-vdb.${STLX} pthread dl m )
    set( COMMON_LIBS_WRITE  ${NCBI_VDB_LIBDIR}/libncbi-wvdb.${STLX} pthread dl m )
endif()

if( WIN32 )
    add_compile_definitions( UNICODE _UNICODE )
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartup" )
    set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
    set( COMMON_LINK_LIBRARIES  ${COMMON_LINK_LIBRARIES} Ws2_32 Crypt32 )
endif()

function( BuildExecutableForTest exe_name sources libraries )
	add_executable( ${exe_name} ${sources} )
	target_link_libraries( ${exe_name} ${libraries} )
endfunction()

function( AddExecutableTest test_name sources libraries )
	BuildExecutableForTest( "${test_name}" "${sources}" "${libraries}" )
	add_test( NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endfunction()
