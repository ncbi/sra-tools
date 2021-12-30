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

add_compile_definitions( __mod__="test/align" )

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/ext/ ) # zlib.h

# AddExecutableTest( Test_Align_load_index "indextest" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

if( NOT WIN32 )
    include_directories( ${VDB_SRCDIR}/libs/align )
    AddExecutableTest( Test_Align_samextract "test-samextract" "${NCBI_VDB_ILIBDIR}/libsam-extract.${STLX};${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
endif()

add_executable( test-estimator pileup_estimator )
include_directories( ${CMAKE_SOURCE_DIR} )
target_link_libraries( test-estimator ncbi-ngs-c++ ngs-c++ ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
add_test( NAME SlowTest_Align_estimator COMMAND test-estimator WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
