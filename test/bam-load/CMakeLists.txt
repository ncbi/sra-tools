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

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    add_test(NAME BamTestSetup COMMAND bash -c "echo 'vdb/schema/paths = \"${VDB_INCDIR}\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-bamfload\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(BamTestSetup PROPERTIES FIXTURES_SETUP BamTest)

    add_test( NAME Test_BamLoader_1
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                bash -c "${BINDIR}/bam-load --accept-dups -Q0 --cache-size 2000000 --tmpfs try_SRR6668252/TMP --xml-log log.xml -o out.sra test.sam"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_BamLoader_1 PROPERTIES FIXTURES_REQUIRED BamTest )

endif()
