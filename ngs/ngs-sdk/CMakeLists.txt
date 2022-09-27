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

add_subdirectory( adapter )
add_subdirectory( dispatch )
add_subdirectory( language )
if ( NOT TOOLS_ONLY )
    add_subdirectory( examples )
endif()

set( NGS_CPP_TARGETS
    ngs-bind-c++
    ngs-disp
)

set( NGS_SDK_TARGETS
    ngs-disp
    ngs-bind-python
)
if ( Java_FOUND )
    set( NGS_SDK_TARGETS ${NGS_SDK_TARGETS} ngs-bind-java )
endif()

set( NCBI_NGS_TARGETS
    ngs
    ncbi-ngs-c++
    ngs-py
    ${NGS_CPP_TARGETS}
)
if ( Java_FOUND )
    set( NCBI_NGS_TARGETS ${NCBI_NGS_TARGETS} ngs-jni )
endif()

# Merging NGS_SDK_TARGETS with NCBI_NGS_TARGETS for ngs-sdk lib to contain all necessary functions for python and java to import (VDB-4713)
set(NGS_SDK_TARGETS ${NGS_SDK_TARGETS} ${NCBI_NGS_TARGETS})

if( SINGLE_CONFIG OR WIN32 )
    include( CMakeLists.posix.txt )
else() # XCode
    include( CMakeLists.xcode.txt )
endif()

install( DIRECTORY ngs
         DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)

