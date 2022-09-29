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

if ( "linux" STREQUAL ${OS} )
if ( EXISTS "${DIRTOTEST}/sam-dump${EXE}" )

	add_executable( sam-factory sam-factory.cpp )

    add_test( NAME Test_sam_dump_verify_omit
        COMMAND ./verify_omit_qual.sh ${DIRTOTEST} ${TESTBINDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_sam_dump_star_quality
        COMMAND ./print_star_on_missing_qual.sh ${DIRTOTEST} ${TESTBINDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
    message(WARNING "${DIRTOTEST}/sam-dump${EXE} is not found. The corresponding tests are skipped." )
endif()
endif()
