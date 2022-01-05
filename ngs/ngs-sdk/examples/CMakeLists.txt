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

include_directories( ${CMAKE_SOURCE_DIR}/ngs )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

if (WIN32)
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup" )
endif()

link_libraries( ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ncbi-ngs-c++ ngs-bind-c++ ngs-disp ngs )

add_executable( FragTest FragTest.cpp )
add_executable( AlignTest AlignTest.cpp )
add_executable( AlignSliceTest AlignSliceTest.cpp )
add_executable( PileupTest PileupTest.cpp )
add_executable( RefTest RefTest.cpp )
add_executable( DumpReferenceFASTA DumpReferenceFASTA.cpp )

if ( NOT WIN32 )
	add_test( NAME SlowTest_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${BINDIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()