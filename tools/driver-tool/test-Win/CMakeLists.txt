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

if(WIN32)
    add_executable(test-Win-proc)

    target_sources(test-Win-proc
        PRIVATE
            test-proc.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../proc.win32.cpp
    )
    target_include_directories(test-Win-proc PRIVATE AFTER ${CMAKE_CURRENT_SOURCE_DIR}/../)
    target_compile_definitions(test-Win-proc PRIVATE USE_WIDE_API=1)

    target_link_libraries(test-Win-proc
        libcmt$<$<CONFIG:Debug>:d>
        libcpmt$<$<CONFIG:Debug>:d>
        libucrt$<$<CONFIG:Debug>:d>
        libvcruntime$<$<CONFIG:Debug>:d>
    )
    add_test( NAME test-Win-proc COMMAND test-Win-proc )
endif()
