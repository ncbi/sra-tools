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

add_executable(sratools)
target_compile_definitions( sratools PRIVATE __mod__="tools/driver-tool" )

target_sources(sratools
    PRIVATE
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
        service.cpp
        service.hpp
        json-parse.cpp
        json-parse.hpp
        SDL-response.cpp
        SDL-response.hpp
        tool-args.cpp
        tool-args.hpp
        globals.hpp
        constants.hpp
        debug.hpp
        utf8-16.hpp
        util.hpp
        file-path.hpp
        opt_string.hpp
        tool-arguments.h
)
if(WIN32)
	target_sources(sratools PRIVATE
        file-path.win32.cpp
        file-path.win32.hpp
        proc.win32.cpp
        proc.win32.hpp
        util.win32.hpp
        wide-char.hpp
        win32-cmdline.hpp
        win32-cmdline.cpp		
	)
else()
	target_sources(sratools PRIVATE
        file-path.posix.cpp
        file-path.posix.hpp
        proc.posix.cpp
        proc.posix.hpp
        util.posix.hpp
	)
endif()
target_link_libraries( sratools
    ncbi-vdb
)
if(WIN32)
    target_compile_definitions(sratools PRIVATE USE_WIDE_API=1)
    target_link_libraries(sratools PRIVATE
        libcmt$<$<CONFIG:Debug>:d>
        libcpmt$<$<CONFIG:Debug>:d>
        libucrt$<$<CONFIG:Debug>:d>
        libvcruntime$<$<CONFIG:Debug>:d>
		Ws2_32 Crypt32 ShLwApi Pathcch
    )
elseif(LINUX)
    target_link_libraries(sratools PRIVATE m)
endif()
MakeLinksExe( sratools false )
