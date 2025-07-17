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

set( compile_defs __mod__="tools/driver-tool" )

set( sources
    sratools.cpp
    sratools.hpp
    config.cpp
    config.hpp
    command-line.cpp
    command-line.hpp
    run-source.cpp
    run-source.hpp
    proc.cpp
    proc.hpp
    uuid.cpp
    uuid.hpp
    accession.cpp
    accession.hpp
    service.cpp
    service.hpp
    json-parse.cpp
    json-parse.hpp
    SDL-response.cpp
    SDL-response.hpp
    tool-args.cpp
    tool-args.hpp
    build-version.cpp
    build-version.hpp
    globals.hpp
    constants.hpp
    debug.hpp
    utf8-16.hpp
    util.hpp
    file-path.hpp
    opt_string.hpp
    common-arguments.h
    fastq-dump-arguments.h
    tool-arguments.h
)

if( CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32 )
    # UPDATING THE TOOL ARGUMENTS HEADER ONLY WORKS IN DEBUG CODE

    add_custom_target(
        tool-arguments-header COMMAND
        sh generate-args-info.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        BYPRODUCTS tool-arguments.h.timestamp
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_dependencies(
        tool-arguments-header
        vdb-dump
        fasterq-dump
        sam-dump
        sra-pileup
        prefetch
    )
endif()

if( WIN32 )
    list(APPEND sources
        file-path.win32.cpp
        proc.win32.cpp
        proc.win32.hpp
        util.win32.hpp
        wide-char.hpp
        win32-cmdline.hpp
    )
else()
    list(APPEND sources
        file-path.posix.cpp
        proc.posix.cpp
        proc.posix.hpp
        util.posix.hpp
    )
endif()

set(link_libs ${COMMON_LIBS_READ})
if( WIN32 )
    list(APPEND compile_defs USE_WIDE_API=1)
    list(APPEND link_libs
        libcmt$<$<CONFIG:Debug>:d>
        libcpmt$<$<CONFIG:Debug>:d>
        libucrt$<$<CONFIG:Debug>:d>
        libvcruntime$<$<CONFIG:Debug>:d>
        Ws2_32 Crypt32 ShLwApi PathCch
    )
else()
    list(APPEND compile_defs PREFIX_PATH="${CMAKE_PREFIX_PATH}" BUILD_PATH="${BINDIR}")
endif()

GenerateExecutableWithDefs(sratools "${sources}" "${compile_defs}" "" "${link_libs}")
MakeLinksExe( sratools false )

if( WIN32 )
    # rename the executables
    foreach (targ vdb-dump fasterq-dump sam-dump sra-pileup)
        add_custom_command(TARGET sratools
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sratools${EXE} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${targ}-driver${EXE}
        )
    endforeach ()
endif()

if ( CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32 )
    add_dependencies(sratools tool-arguments-header)
endif()
