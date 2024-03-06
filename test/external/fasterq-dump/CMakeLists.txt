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

if ( NOT WIN32 )

ToolsRequired(fasterq-dump sam-factory bam-load kar)

    add_test( NAME Test_FasterqDump_Help
        COMMAND ${DIRTOTEST}/fasterq-dump -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_FasterqDump_NotZeroWithoutParameters
        COMMAND ${DIRTOTEST}/fasterq-dump
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_FasterqDump_NotZeroWithoutParameters PROPERTIES WILL_FAIL TRUE)

    add_test( NAME Test_FasterDump_Fasta_unsorted_flat_tbl_read_id
        COMMAND
            bash test_for_read_id_flat.sh ${DIRTOTEST}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_FasterDump_Fasta_unsorted_cSRA_read_id
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
            bash -c "echo 'vdb/schema/paths = \"${VDB_INCDIR}\"' > tmp.kfg; ./test_for_read_id_cSRA.sh ${DIRTOTEST}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    if( RUN_SANITIZER_TESTS )
        add_test( NAME Test_FasterqDump_Help-asan
            COMMAND ${BINDIR}/fasterq-dump-asan -h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        add_test( NAME Test_FasterqDump_NotZeroWithoutParameters-asan
            COMMAND ${BINDIR}/fasterq-dump-asan
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        set_tests_properties(Test_FasterqDump_NotZeroWithoutParameters-asan PROPERTIES WILL_FAIL TRUE)

        add_test( NAME Test_FasterqDump_Help-tsan
            COMMAND ${BINDIR}/fasterq-dump-tsan -h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        add_test( NAME Test_FasterqDump_NotZeroWithoutParameters-tsan
            COMMAND ${BINDIR}/fasterq-dump-tsan
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        set_tests_properties(Test_FasterqDump_NotZeroWithoutParameters-tsan PROPERTIES WILL_FAIL TRUE)
    endif()

    add_test( NAME Test_Tiny_cSRA
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
            bash -c "echo 'vdb/schema/paths = \"${VDB_INCDIR}\"' > tmp.kfg; ./tiny_csra.sh ${DIRTOTEST} ${BINDIR}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()
