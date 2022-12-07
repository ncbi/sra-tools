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

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    add_test(NAME SamDumpTestSetup COMMAND bash -c "echo 'vdb/schema/paths = \"${VDB_INCDIR}\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-sam-dump-test\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(SamDumpTestSetup PROPERTIES FIXTURES_SETUP SamDumpTest)

    add_test( NAME Test_sam_dump_verify_omit
        COMMAND
            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
            ./verify_omit_qual.sh ${DIRTOTEST}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties( Test_sam_dump_verify_omit PROPERTIES FIXTURES_REQUIRED SamDumpTest )

    add_test( NAME Test_sam_dump_star_quality
        COMMAND
            ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
            ./print_star_on_missing_qual.sh ${DIRTOTEST}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties( Test_sam_dump_star_quality PROPERTIES FIXTURES_REQUIRED SamDumpTest )

else()
    message(WARNING "${DIRTOTEST}/sam-dump${EXE} is not found. The corresponding tests are skipped." )
endif()
endif()
