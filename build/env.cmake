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

include(CheckCXXCompilerFlag)

# allow implicit source file extensions
if ( ${CMAKE_VERSION} VERSION_EQUAL "3.20" OR
     ${CMAKE_VERSION} VERSION_GREATER "3.20")
    cmake_policy(SET CMP0115 OLD)
endif()

# ===========================================================================
# set of build targets

# external tools are always included
set(BUILD_TOOLS_INTERNAL    "OFF" CACHE STRING "If set to ON, build internal tools")
set(BUILD_TOOLS_LOADERS     "OFF" CACHE STRING "If set to ON, build loaders")
set(BUILD_TOOLS_TEST_TOOLS  "OFF" CACHE STRING "If set to ON, build test tools")
set(TOOLS_ONLY              "OFF" CACHE STRING "If set to ON, generate tools targets only")

message( "BUILD_TOOLS_INTERNAL=${BUILD_TOOLS_INTERNAL}" )
message( "BUILD_TOOLS_LOADERS=${BUILD_TOOLS_LOADERS}" )
message( "BUILD_TOOLS_TEST_TOOLS=${BUILD_TOOLS_TEST_TOOLS}" )
message( "TOOLS_ONLY=${TOOLS_ONLY}" )

# ===========================================================================

set( VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shared/toolkit.vers")
file( READ ${VERSION_FILE} VERSION )
string( STRIP ${VERSION} VERSION )
message( VERSION=${VERSION} )
string( REGEX MATCH "^[0-9]+" MAJVERS ${VERSION} )

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# determine OS
if ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Darwin" )
    set(OS "mac")
    set(LIBPFX "lib")
    set(SHLX "dylib")
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "FreeBSD" )
    set(OS "bsd")
    set(LIBPFX "lib")
    set(SHLX "so")
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Linux" )
    set(OS "linux")
    set(LIBPFX "lib")
    set(SHLX "so")
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "NetBSD" )
    set(OS "bsd")
    set(LIBPFX "lib")
    set(SHLX "so")
    set(STLX "a")
elseif ( ${CMAKE_HOST_SYSTEM_NAME} STREQUAL  "Windows" )
    set(OS "windows")
    set(LIBPFX "")
    set(STLX "lib")
else()
    message ( FATAL_ERROR "unknown OS " ${CMAKE_HOST_SYSTEM_NAME})
endif()

# determine architecture
if ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "armv7l")
	set(ARCH "armv7l")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "arm64")
    set(ARCH "arm64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    set(ARCH "arm64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(ARCH "x86_64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "amd64")
    set(ARCH "x86_64")
elseif ( ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(ARCH "x86_64")
else()
    message ( FATAL_ERROR "unknown architecture " ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif ()

if( "arm64" STREQUAL "${ARCH}" )
    # for bitmagic SIMD
    add_compile_definitions(BMNEONOPT)
    add_compile_definitions(__arm64__)
endif()

# create variables based entirely upon OS
if ( "bsd" STREQUAL ${OS} )
    add_compile_definitions( BSD UNIX )
    set( LMCHECK "" )
    set( EXE "" )
elseif ( "mac" STREQUAL ${OS} )
    add_compile_definitions( MAC BSD UNIX )
    set( LMCHECK "" )
    set( EXE "" )
elseif( "linux" STREQUAL ${OS} )
    add_compile_definitions( LINUX UNIX )
    set( LMCHECK -lmcheck )
    set( EXE "" )
elseif( "windows" STREQUAL ${OS} )
    add_compile_definitions( WINDOWS _WIN32_WINNT=0x0600 )
    set( LMCHECK "" )
    set( EXE ".exe" )
endif()

# create variables based entirely upon ARCH
# TODO: check if we need anything besides BITS in the if-else below
if ("armv7l" STREQUAL ${ARCH})
	set( BITS 32 )
	add_compile_options( -Wno-psabi )
elseif ("aarch64" STREQUAL ${ARCH} OR "arm64" STREQUAL ${ARCH})
	set ( BITS 64 )
elseif ("x86_64" STREQUAL ${ARCH} )
    set ( BITS 64 )
endif()

# now any unique combinations of OS and ARCH
# TODO: check if this is needed
if     ( "mac-x86_84" STREQUAL ${OS}-${ARCH})
elseif ( "linux-x86_64" STREQUAL ${OS}-${ARCH})
elseif ( "linux-armv7l" STREQUAL ${OS}-${ARCH})
elseif ( "linux-arm64" STREQUAL ${OS}-${ARCH})
    add_compile_definitions( __float128=_Float128 )
endif()

# combinations of OS and COMP
if ( ${OS}-${CMAKE_CXX_COMPILER_ID} STREQUAL "linux-GNU"  )
    add_definitions( -rdynamic )
    add_compile_definitions( _GNU_SOURCE )
endif()

add_compile_definitions( _ARCH_BITS=${BITS} ${ARCH} ) # TODO ARCH ?

# global compiler warnings settings
if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
    set( DISABLED_WARNINGS_C "-Wno-unused-function")
    set( DISABLED_WARNINGS_CXX )
elseif ( CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$" )
    set( DISABLED_WARNINGS_C "-Wno-unused-function")
    set( DISABLED_WARNINGS_CXX "-Wno-tautological-undefined-compare")
elseif ( "MSVC" STREQUAL "${CMAKE_C_COMPILER_ID}")
    #
    # Unhelpful warnings, generated in particular by MSVC and Windows SDK header files
    #
    # Warning C4820: 'XXX': 'N' bytes padding added after data member 'YYY'
    # Warning C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    # Warning C4668: 'XXX' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
    # Warning C5105: macro expansion producing 'defined' has undefined behavior
    # Warning C4514: 'XXX': unreferenced inline function has been removed
    # warning C4623: 'XXX': default constructor was implicitly defined as deleted
    # warning C4625: 'XXX': copy constructor was implicitly defined as deleted
    # warning C4626: 'XXX': assignment operator was implicitly defined as deleted
    # warning C5026: 'XXX': move constructor was implicitly defined as deleted
    # warning C5027: 'XXX': move assignment operator was implicitly defined as deleted
    # warning C4571: Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
    # warning C4774: '_scprintf' : format string expected in argument 1 is not a string literal
    # warning C4255: 'XXX': no function prototype given: converting '()' to '(void)'
    # warning C4710: 'XXX': function not inlined
    # warning C5031: #pragma warning(pop): likely mismatch, popping warning state pushed in different file
    # warning C5032: detected #pragma warning(push) with no corresponding #pragma warning(pop)
    set( DISABLED_WARNINGS_C "/wd4820 /wd5045 /wd4668 /wd5105 /wd4514 /wd4774 /wd4255 /wd4710 /wd5031 /wd5032")
    set( DISABLED_WARNINGS_CXX "/wd4623 /wd4625 /wd4626 /wd5026 /wd5027 /wd4571")
    add_compile_options(/W4)
else()
    add_compile_options(-Wall)
endif()
set( CMAKE_C_FLAGS "-Wall ${CMAKE_C_FLAGS} ${DISABLED_WARNINGS_C}" )
set( CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS} ${DISABLED_WARNINGS_C} ${DISABLED_WARNINGS_CXX}" )

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

set(COMPILER_OPTION_SSE42_SUPPORTED OFF)
if( "x86_64" STREQUAL ${ARCH} )
    CHECK_CXX_COMPILER_FLAG("-msse4.2" COMPILER_OPTION_SSE42_SUPPORTED)
endif()
# if (COMPILER_OPTION_SSE42_SUPPORTED)
#     message("compiler accepts sse4.2 flag")
# else()
#     message("compiler does not accept sse4.2 flag")
# endif()

#message( "OS=" ${OS} " ARCH=" ${ARCH} " CXX=" ${CMAKE_CXX_COMPILER} " LMCHECK=" ${LMCHECK} " BITS=" ${BITS} " CMAKE_C_COMPILER_ID=" ${CMAKE_C_COMPILER_ID} " CMAKE_CXX_COMPILER_ID=" ${CMAKE_CXX_COMPILER_ID} )

# ===========================================================================
# 3d party packages

# Flex/Bison
find_package( FLEX 2.6 )
find_package( BISON 3 )

if (XML2_LIBDIR)
    find_library( LIBXML2_LIBRARIES libxml2.a HINTS ${XML2_LIBDIR} )
    if ( LIBXML2_LIBRARIES )
        set( LibXml2_FOUND true )
        if ( XML2_INCDIR )
            set( LIBXML2_INCLUDE_DIR ${XML2_INCDIR} )
        endif()
    endif()
else()
    find_package( LibXml2 )
endif()
if( LibXml2_FOUND )
    message( LIBXML2_INCLUDE_DIR=${LIBXML2_INCLUDE_DIR} )
    message( LIBXML2_LIBRARIES=${LIBXML2_LIBRARIES} )
endif()

if(NO_JAVA)
else()
    find_package(Java COMPONENTS Development)
    if( Java_FOUND AND NOT Java_VERSION )
        message(STATUS "No version of Java found")
        unset( Java_FOUND )
    endif()
endif()

if ( PYTHON_PATH )
    set( Python3_EXECUTABLE ${PYTHON_PATH} )
endif()
find_package( Python3 COMPONENTS Interpreter )

# ===========================================================================
# testing

enable_testing()

option(COVERAGE "Generate test coverage" OFF)

if( COVERAGE AND "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
    message( COVERAGE=${COVERAGE} )

    SET(GCC_COVERAGE_COMPILE_FLAGS "-coverage -fprofile-arcs -ftest-coverage")
    SET(GCC_COVERAGE_LINK_FLAGS    "-coverage -lgcov")

    SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
    SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )
endif()

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

# VDB-4651 - relying on ./configure's logic for determining interfaces location
set( VDB_INTERFACES_DIR "${VDB_INCDIR}" )
if ( NOT VDB_INTERFACES_DIR OR NOT EXISTS ${VDB_INTERFACES_DIR} )
	message(FATAL_ERROR "VDB_INCDIR=\"${VDB_INTERFACES_DIR}\" does not exist - ncbi-vdb was not installed in that location. VDB_INCDIR variable pointing to the ncbi-vdb headers (interfaces) must be specified.")
else()
	message("Using ncbi-vdb interfaces: ${VDB_INTERFACES_DIR}")
endif()

if ( ${CMAKE_GENERATOR} MATCHES "Visual Studio.*" OR
     ${CMAKE_GENERATOR} STREQUAL "Xcode" )
    set( SINGLE_CONFIG false )

    if (VDB_BINDIR)
        message( "Using ncbi-vdb build in ${VDB_BINDIR}.")
    endif ()
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
    set( TESTBINDIR_DEBUG "${TEST_RUNTIME_OUTPUT_DIRECTORY_DEBUG}" )
    set( TESTBINDIR_RELEASE "${TEST_RUNTIME_OUTPUT_DIRECTORY_RELEASE}" )
    set( TEMPDIR "$<$<CONFIG:Debug>:${TEMPDIR_DEBUG}>$<$<CONFIG:Release>:${TEMPDIR_RELEASE}>" )

    link_directories( $<$<CONFIG:Debug>:${NCBI_VDB_LIBDIR_DEBUG}> $<$<CONFIG:Release>:${NCBI_VDB_LIBDIR_RELEASE}> )
    link_directories( $<$<CONFIG:Debug>:${NCBI_VDB_ILIBDIR_DEBUG}> $<$<CONFIG:Release>:${NCBI_VDB_ILIBDIR_RELEASE}> )

else() # assume a single-config generator
    set( SINGLE_CONFIG true )

	if( NOT VDB_LIBDIR OR NOT EXISTS ${VDB_LIBDIR} )
		message(FATAL_ERROR "VDB_LIBDIR=\"${VDB_LIBDIR}\" does not exist - ncbi-vdb was not installed in that location. VDB_LIBDIR variable pointing to the ncbi-vdb binary libraries must be specified.")
	else()
		message("Using ncbi-vdb binary libraries: ${VDB_LIBDIR}")
	endif()

    set( NCBI_VDB_LIBDIR ${VDB_LIBDIR} )

    SetAndCreate( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TARGDIR}/bin )
    SetAndCreate( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${TARGDIR}/lib )
    SetAndCreate( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${TARGDIR}/ilib )
    set( CMAKE_JAR_OUTPUT_DIRECTORY              ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )

    # to be used in add-test() as the location of executables.
    # NOTE: always use the COMMAND_EXPAND_LISTS option of add_test
    set( BINDIR "${TARGDIR}/bin" )
    set( LIBDIR "${TARGDIR}/lib" )
    set( TESTBINDIR "${TARGDIR}/test-bin" )
    set( TESTBINDIR_DEBUG "${TESTBINDIR}" )
    set( TESTBINDIR_RELEASE "${TESTBINDIR}" )
    SetAndCreate( TEMPDIR "${TESTBINDIR}/tmp" )

    link_directories( ${NCBI_VDB_LIBDIR} ) # Must point to the installed ncbi-vdb libs
endif()

# ===========================================================================
# ncbi-vdb sources

# Using installed ncbi-vdb directory
if( WIN32 )
	# TODO: WIN32 and Mac still work in an assumption that ncbi-vdb sources are checked out alongside with sra-tools
	set( USE_INSTALLED_NCBI_VDB 0 )
elseif( SINGLE_CONFIG )
	set( USE_INSTALLED_NCBI_VDB 1 )
else() # XCode
	# TODO: WIN32 and Mac still work in an assumption that ncbi-vdb sources are checked out alongside with sra-tools
	set( USE_INSTALLED_NCBI_VDB 0 )
        # Workaround for Xcode's signing phase code-braking behavior
        set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "-o 131072" ) # linker-signed
endif()


include_directories( ${VDB_INTERFACES_DIR} )

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

if ( "bsd" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/bsd)
    include_directories(${VDB_INTERFACES_DIR}/os/unix)
elseif ( "mac" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/mac)
    include_directories(${VDB_INTERFACES_DIR}/os/unix)
elseif( "linux" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/linux)
    include_directories(${VDB_INTERFACES_DIR}/os/unix)
elseif( "windows" STREQUAL ${OS} )
    include_directories(${VDB_INTERFACES_DIR}/os/win)
endif()

include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )
include_directories( ${CMAKE_SOURCE_DIR}/libs/inc )

# path to schema includes residing in sra-tools
set( SRC_INTERFACES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/schema" )


# ===========================================================================

# DIRTOTEST is the overridable location of the executables to call from scripted test
if( NOT DIRTOTEST )
    set( DIRTOTEST ${BINDIR} )
endif()
#message( DIRTOTEST: ${DIRTOTEST})

# CONFIGTOUSE is a way to block user settings ($HOME/.ncbi/user-settings.mkfg). Assign anything but NCBI_SETTINGS to it, and the user settings will be ignored.
if( NOT CONFIGTOUSE )
    set( CONFIGTOUSE NCBI_SETTINGS )
endif()
#message( CONFIGTOUSE: ${CONFIGTOUSE})

find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")

if( Python3_EXECUTABLE )
 if( NOT LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu" )
  # create virtual environment
  execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m venv "${CMAKE_BINARY_DIR}/venv" )

  # update the environment with VIRTUAL_ENV variable (mimic the activate script)
  set( ENV{VIRTUAL_ENV} "${CMAKE_BINARY_DIR}/venv" )

  # change the context of the search
  set( Python3_FIND_VIRTUALENV FIRST )

  # unset Python3_EXECUTABLE because it is also an input variable
  # (see documentation, Artifacts Specification section)
  unset( Python3_EXECUTABLE )

  # Launch a new search
  find_package( Python3 COMPONENTS Interpreter Development )
# message(STATUS "Use: ${Python3_EXECUTABLE}")

  execute_process( COMMAND
         "${Python3_EXECUTABLE}" -m pip install --upgrade pip setuptools wheel )
 endif()
endif()

#
# Common functions for creation of build artefacts
#

# provide ability to override installation directories
if ( NOT CMAKE_INSTALL_BINDIR )
    set( CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin )
endif()

if ( NOT CMAKE_INSTALL_LIBDIR )
    set( CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib64 )
endif()

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
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
         endif()
    else()
        set_target_properties( ${name} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG})
        set_target_properties( ${name} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE})
        if ( ${install} )
            install( TARGETS ${name} DESTINATION ${CMAKE_INSTALL_LIBDIR} )
        endif()
    endif()
endfunction()


#
# create versioned names and symlinks for a shared library
#
function(MakeLinksShared target name install)
    if( SINGLE_CONFIG )
        if( ${OS} STREQUAL "mac" )
            set( LIBSUFFIX ".${VERSION}.${SHLX}" )
            set( MAJLIBSUFFIX ".${MAJVERS}.${SHLX}" )
        else()
            set( LIBSUFFIX ".${SHLX}.${VERSION}" )
            set( MAJLIBSUFFIX ".${SHLX}.${MAJVERS}" )
        endif()
        add_custom_command(TARGET ${target}
            POST_BUILD
            COMMAND rm -f lib${name}${LIBSUFFIX}
            COMMAND mv lib${name}.${SHLX} lib${name}${LIBSUFFIX}
            COMMAND ln -f -s lib${name}${LIBSUFFIX} lib${name}${MAJLIBSUFFIX}
            COMMAND ln -f -s lib${name}${MAJLIBSUFFIX} lib${name}.${SHLX}
            WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        )

        set_property(
            TARGET    ${target}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}${LIBSUFFIX};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}${MAJLIBSUFFIX};${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}"
        )

        if ( ${install} )
            install( PROGRAMS  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}${LIBSUFFIX}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}${MAJLIBSUFFIX}
                            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${name}.${SHLX}
                    DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
        endif()
    else()
        set_target_properties( ${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
        set_target_properties( ${target} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
        if ( ${install} )
            install( TARGETS ${target}
                     ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
                     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        endif()

        if (WIN32)
            install(FILES $<TARGET_PDB_FILE:${target}> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
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

include(CheckCXXSourceRuns)

#
# Ensure static linking against C/C++ runtime.
# Create versioned names and symlinks for an executable.
#

if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}" )
    # check for the presence of static C/C++ runtime libraries
    set(CMAKE_REQUIRED_LINK_OPTIONS -static-libgcc)
    check_cxx_source_runs("int main(int argc, char *argv[]) { return 0; }" HAVE_STATIC_LIBGCC)
    set(CMAKE_REQUIRED_LINK_OPTIONS -static-libstdc++)
    check_cxx_source_runs("int main(int argc, char *argv[]) { return 0; }" HAVE_STATIC_LIBSTDCXX)
endif()

function(MakeLinksExe target install_via_driver)

    if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}" )
        if ( HAVE_STATIC_LIBGCC )
            target_link_options( ${target} PRIVATE -static-libgcc )
        endif()
        if ( HAVE_STATIC_LIBSTDCXX )
            target_link_options( ${target} PRIVATE -static-libstdc++ )
        endif()
    endif()

# creates dependency loops
#     if ( install_via_driver )
#         add_dependencies( ${target} sratools )
#     endif()

    if( SINGLE_CONFIG )

        if ( install_via_driver )

            add_custom_command(TARGET ${target}
                POST_BUILD
                COMMAND rm -f ${target}.${VERSION}
                COMMAND mv ${target} ${target}.${VERSION}
                COMMAND ln -f -s ${target}.${VERSION} ${target}.${MAJVERS}
                COMMAND ln -f -s ${target}.${MAJVERS} ${target}
                COMMAND ln -f -s sratools.${VERSION} ${target}-driver
                WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            )

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
            add_custom_command(TARGET ${target}
                POST_BUILD
                COMMAND rm -f ${target}.${VERSION}
                COMMAND mv ${target} ${target}.${VERSION}
                COMMAND ln -f -s ${target}.${VERSION} ${target}.${MAJVERS}
                COMMAND ln -f -s ${target}.${MAJVERS} ${target}
                WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            )

            install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${VERSION}
                              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${MAJVERS}
                              ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}
                    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
            )
        endif()

        set_property(
            TARGET    ${target}
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${VERSION};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${MAJVERS};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target};${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}-driver"
        )

    else()

        if ( install_via_driver )
                # on Windows/XCode, ${target}-orig file names have no version attached
                install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}${EXE}
                         RENAME ${target}-orig${EXE}
                         DESTINATION ${CMAKE_INSTALL_BINDIR}
                )
                install( PROGRAMS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sratools${EXE}
                         RENAME ${target}${EXE}
                         DESTINATION ${CMAKE_INSTALL_BINDIR}
                )

                if (WIN32)
                    # on install, copy/rename the .pdb files if any
                    install(FILES $<TARGET_PDB_FILE:${target}>
                            RENAME ${target}-orig.pdb
                            DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
                    # add the driver-tool's .pdb
                    install(FILES $<TARGET_PDB_FILE:sratools>
                            RENAME ${target}.pdb
                            DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
                endif()

        else()

            install( TARGETS ${target} DESTINATION ${CMAKE_INSTALL_BINDIR} )

            if (WIN32) # copy the .pdb files if any
                install(FILES $<TARGET_PDB_FILE:${target}> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
            endif()

        endif()

    endif()
endfunction()

include(CheckIncludeFileCXX)
unset(HAVE_MBEDTLS_H CACHE) # TODO: remove
unset(HAVE_MBEDTLS_F CACHE) # TODO: remove
check_include_file_cxx(mbedtls/md.h HAVE_MBEDTLS_H)
if ( HAVE_MBEDTLS_H )
	set( MBEDTLS_LIBS mbedx509 mbedtls mbedcrypto )
	set(CMAKE_REQUIRED_LIBRARIES ${MBEDTLS_LIBS})
	include(CheckCXXSourceRuns)
	check_cxx_source_runs("
#include <stdio.h>
#include \"mbedtls/md.h\"
#include \"mbedtls/sha256.h\"
int main(int argc, char *argv[]) {
	mbedtls_md_context_t ctx;
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
	mbedtls_md_init(&ctx);
	printf(\"test p: %p\", ctx.md_ctx);
}
" HAVE_MBEDTLS_F)
endif()

if( NOT SINGLE_CONFIG )
#    set( COMMON_LINK_LIBRARIES kapp tk-version )
    set( COMMON_LINK_LIBRARIES tk-version )
    set( COMMON_LIBS_READ  $<$<CONFIG:Debug>:${NCBI_VDB_LIBDIR_DEBUG}>$<$<CONFIG:Release>:${NCBI_VDB_LIBDIR_RELEASE}>/${LIBPFX}ncbi-vdb.${STLX} ${MBEDTLS_LIBS} )
    set( COMMON_LIBS_WRITE $<$<CONFIG:Debug>:${NCBI_VDB_LIBDIR_DEBUG}>$<$<CONFIG:Release>:${NCBI_VDB_LIBDIR_RELEASE}>/${LIBPFX}ncbi-wvdb.${STLX} ${MBEDTLS_LIBS} )
else()
    # single-config generators need full path to ncbi-vdb libraries in order to handle the dependency correctly
#    set( COMMON_LINK_LIBRARIES ${NCBI_VDB_LIBDIR}/libkapp.${STLX} tk-version )
    set( COMMON_LINK_LIBRARIES tk-version )
    set( COMMON_LIBS_READ   ${NCBI_VDB_LIBDIR}/libncbi-vdb.${STLX} pthread dl m ${MBEDTLS_LIBS} )
    set( COMMON_LIBS_WRITE  ${NCBI_VDB_LIBDIR}/libncbi-wvdb.${STLX} pthread dl m ${MBEDTLS_LIBS} )
endif()

if( WIN32 )
    add_compile_options( /Zc:__cplusplus )      # enable C++ version variable
    add_compile_definitions( UNICODE _UNICODE USE_WIDE_API )
    #set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartup" )
    set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
    set( COMMON_LINK_LIBRARIES  ${COMMON_LINK_LIBRARIES} Ws2_32 Crypt32 ${MBEDTLS_LIBS} )
endif()

if ( SINGLE_CONFIG )
    # standard kfg files
    if ( BUILD_TOOLS_INTERNAL )
        set( VDB_COPY_DIR ${CMAKE_SOURCE_DIR}/tools/internal/vdb-copy )
    endif()
    install( SCRIPT CODE
        "execute_process( COMMAND /bin/sh -c        \
            \"${CMAKE_SOURCE_DIR}/build/install.sh  \
                ${VDB_INCDIR}/kfg/ncbi              \
                '${VDB_COPY_DIR}'                   \
                ${CMAKE_INSTALL_PREFIX}/bin/ncbi    \
                /etc/ncbi                           \
                ${CMAKE_INSTALL_BINDIR}             \
                ${CMAKE_INSTALL_LIBDIR}             \
                ${CMAKE_SOURCE_DIR}/shared/kfgsums  \
            \" )"
    )
endif()

execute_process( COMMAND sh -c "${CMAKE_CXX_COMPILER} -fsanitize=address test.cpp && ./a.out"
    RESULT_VARIABLE ASAN_WORKS
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build  )
execute_process( COMMAND sh -c "${CMAKE_CXX_COMPILER} -fsanitize=thread test.cpp && ./a.out"
    RESULT_VARIABLE TSAN_WORKS
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build  )

if( ASAN_WORKS EQUAL 0 AND TSAN_WORKS EQUAL 0 )

    if( NOT SINGLE_CONFIG AND RUN_SANITIZER_TESTS )
        message( "RUN_SANITIZER_TESTS (${RUN_SANITIZER_TESTS}) cannot be turned on in a non single config mode - overriding to OFF" )
        set( RUN_SANITIZER_TESTS OFF )
    endif()

    if( RUN_SANITIZER_TESTS_OVERRIDE )
        message("Overriding sanitizer tests due to RUN_SANITIZER_TESTS_OVERRIDE: ${RUN_SANITIZER_TESTS_OVERRIDE}")
        set( RUN_SANITIZER_TESTS ON )
    endif()

    #
    # TSAN-instrumented programs used to crash on starup with certain version of Ubuntu kernel. Seems to be not thecase anymore, so disabling this section.
    #
    # if( RUN_SANITIZER_TESTS )
    #     find_program(LSB_RELEASE_EXEC lsb_release)
    #     execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    #         OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    #         OUTPUT_STRIP_TRAILING_WHITESPACE
    #     )
    #     message("LSB_RELEASE_ID_SHORT: ${LSB_RELEASE_ID_SHORT}")
    #     if( LSB_RELEASE_ID_SHORT STREQUAL "Ubuntu" )
    #         message("Disabling sanitizer tests on Ubuntu...")
    #         set( RUN_SANITIZER_TESTS OFF )
    #     endif()
    # endif()

else()

    message("ASAN suport is not detected. Disabling sanitizer tests.")
    set( RUN_SANITIZER_TESTS OFF )
    set( RUN_SANITIZER_TESTS_OVERRIDE OFF )

endif()
message( "RUN_SANITIZER_TESTS: ${RUN_SANITIZER_TESTS}" )

function( GenerateStaticLibsWithDefs target_name sources compile_defs include_dirs )
    add_library( ${target_name} STATIC ${sources} )
    if( RUN_SANITIZER_TESTS )
        add_library( "${target_name}-asan" STATIC ${sources} )
        target_compile_options( "${target_name}-asan" PUBLIC -fsanitize=address )
        target_link_options( "${target_name}-asan" PUBLIC -fsanitize=address )

        add_library( "${target_name}-tsan" STATIC ${sources} )
        target_compile_options( "${target_name}-tsan" PUBLIC -fsanitize=thread )
        target_link_options( "${target_name}-tsan" PUBLIC -fsanitize=thread )
    endif()

    if( NOT "" STREQUAL "${compile_defs}" )
        target_compile_definitions( ${target_name} PRIVATE ${compile_defs} )
        if( RUN_SANITIZER_TESTS )
            target_compile_definitions( "${target_name}-asan" PRIVATE ${compile_defs} )
            target_compile_definitions( "${target_name}-tsan" PRIVATE ${compile_defs} )
        endif()
    endif()
    if( NOT "" STREQUAL "${include_dirs}" )
        target_include_directories( ${target_name} PUBLIC "${include_dirs}" )
        if( RUN_SANITIZER_TESTS )
            target_include_directories( "${target_name}-asan" PUBLIC "${include_dirs}" )
            target_include_directories( "${target_name}-tsan" PUBLIC "${include_dirs}" )
        endif()
    endif()
endfunction()

function( GenerateStaticLibs target_name sources )
    GenerateStaticLibsWithDefs( ${target_name} "${sources}" "" "" )
endfunction()

function( GenerateExecutableWithDefs target_name sources compile_defs include_dirs link_libs )
    add_executable( ${target_name} ${sources} )

    # always link as c++
    set_target_properties(${target_name} PROPERTIES LINKER_LANGUAGE CXX)
    if( WIN32 )
        #target_link_options( ${target_name} PRIVATE "/ENTRY:wmainCRTStartup" )
        target_compile_definitions( ${target_name} PRIVATE UNICODE _UNICODE USE_WIDE_API )
    endif()

    if (RUN_SANITIZER_TESTS)
        add_executable( "${target_name}-asan" ${sources} )
        set_target_properties("${target_name}-asan" PROPERTIES LINKER_LANGUAGE CXX)
        target_compile_options( "${target_name}-asan" PRIVATE -fsanitize=address )
        target_link_options( "${target_name}-asan" PRIVATE -fsanitize=address )

        add_executable( "${target_name}-tsan" ${sources} )
        set_target_properties("${target_name}-tsan" PROPERTIES LINKER_LANGUAGE CXX)
        target_compile_options( "${target_name}-tsan" PRIVATE -fsanitize=thread )
        target_link_options( "${target_name}-tsan" PRIVATE -fsanitize=thread )
    endif()

    if( NOT "" STREQUAL "${compile_defs}" )
        target_compile_definitions( ${target_name} PRIVATE "${compile_defs}" )
        if (RUN_SANITIZER_TESTS)
            target_compile_definitions( "${target_name}-asan" PRIVATE "${compile_defs}" )
            target_compile_definitions( "${target_name}-tsan" PRIVATE "${compile_defs}" )
        endif()
    endif()
    if( NOT "" STREQUAL "${include_dirs}" )
        target_include_directories( ${target_name} PRIVATE "${include_dirs}" )
        if (RUN_SANITIZER_TESTS)
            target_include_directories( "${target_name}-asan" PRIVATE "${include_dirs}" )
            target_include_directories( "${target_name}-tsan" PRIVATE "${include_dirs}" )
        endif()
    endif()
    if( NOT "" STREQUAL "${link_libs}" )
        target_link_libraries( ${target_name} "${link_libs}" )
        if (RUN_SANITIZER_TESTS)
            target_link_libraries( "${target_name}-asan" "${link_libs}" )
            target_link_libraries( "${target_name}-tsan" "${link_libs}" )
        endif()
    endif()
endfunction()

function( AddExecutableTest test_name sources libraries include_dirs )
	GenerateExecutableWithDefs( "${test_name}" "${sources}" "" "${include_dirs}" "${libraries}" )
    if( WIN32 )
        target_link_options( ${test_name} PRIVATE "/ENTRY:mainCRTStartup" )
    endif()

	add_test( NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( RUN_SANITIZER_TESTS )
	    if (NOT "--no-asan" IN_LIST ARGV)
    		add_test( NAME "${test_name}-asan" COMMAND "${test_name}-asan" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    	endif()
	    if (NOT "--no-tsan" IN_LIST ARGV)
    		add_test( NAME "${test_name}-tsan" COMMAND "${test_name}-tsan" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    	endif()
	endif()
endfunction()

# use to make sure a test script has the tool(s) it needs built before it executes
macro(ToolsRequired tool) # may provide additional tools
    add_custom_target(Build-${tool} ALL )
    foreach(loop_var ${ARGV})
        add_dependencies(Build-${tool} ${loop_var})
    endforeach()
endmacro()
