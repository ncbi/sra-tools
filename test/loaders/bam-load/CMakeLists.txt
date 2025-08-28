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

if( NOT WIN32 )

    ToolsRequired(bam-load)

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    add_test(NAME BamTestSetup COMMAND sh -c "echo 'vdb/schema/paths = \"${CMAKE_SOURCE_DIR}/libs/schema:${VDB_INCDIR}\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-bamfload\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(BamTestSetup PROPERTIES FIXTURES_SETUP BamTest)

    add_test( NAME Test_BamLoader_1
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/bam-load -o out.sra test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_1 PROPERTIES FIXTURES_REQUIRED BamTest )

    #####################
    # min-batch-size option
    add_test( NAME Test_BamLoader_MinBatchSize
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/bam-load -o out.sra --min-batch-size 999999 test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_MinBatchSize PROPERTIES FIXTURES_REQUIRED BamTest )
    #
    # expected to fail:
    add_test( NAME Test_BamLoader_MinBatchSize_Bad
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/bam-load -o out.sra --min-batch-size notanumber test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_MinBatchSize_Bad PROPERTIES FIXTURES_REQUIRED BamTest WILL_FAIL TRUE )

    #
    # expected to fail:
    add_test( NAME Test_BamLoader_FewerThan3Threads
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/bam-load -o out.sra --threads=2 test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_FewerThan3Threads PROPERTIES FIXTURES_REQUIRED BamTest WILL_FAIL TRUE )

    add_test( NAME Test_BamLoader_ThreadsBad
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/bam-load -o out.sra --threads=N test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_ThreadsBad PROPERTIES FIXTURES_REQUIRED BamTest WILL_FAIL TRUE )
    #####################

    if( TARGET bam-load-asan )
        add_test( NAME Test_BamLoader_1_asan
                COMMAND
                    ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                    ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                    sh -c "${BINDIR}/bam-load-asan -o out.sra test.sam"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        set_tests_properties( Test_BamLoader_1_asan PROPERTIES FIXTURES_REQUIRED BamTest )
    endif()

# See VDB-6090
#     if( TARGET bam-load-tsan )
#         add_test( NAME Test_BamLoader_1_tsan
#                 COMMAND
#                     ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
#                     ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
#                     sh -c "${BINDIR}/bam-load-tsan -o out.sra test.sam"
#                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#         )
#         set_tests_properties( Test_BamLoader_1_tsan PROPERTIES FIXTURES_REQUIRED BamTest )
#     endif()

    AddExecutableTest( Test_BamLoader_platform sam-platform.cpp "" "" )
endif()
