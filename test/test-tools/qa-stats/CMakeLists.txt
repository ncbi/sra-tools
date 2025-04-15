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

    make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

    AddExecutableTest( Test_qa-stats_Input "test-input;../../../tools/test-tools/qa-stats/input.cpp" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "" )
    AddExecutableTest( Test_qa-stats_Output "test-output" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "" )
    AddExecutableTest( Test_qa-stats_Fingerprint "test-finger" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "" )
    AddExecutableTest( Test_qa-stats_Hashing "test-hashing" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "" )

    add_test( NAME Test_qa-stats
            COMMAND runtests.sh "${BINDIR}" "${CMAKE_SOURCE_DIR}/tools/test-tools/qa-stats"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties ( Test_qa-stats
        PROPERTIES
            SKIP_REGULAR_EXPRESSION "^Skipped"
    )

    # parsing fasta/fastq
    file(GLOB INPUTS input/*.fast[aq])
    foreach(i ${INPUTS})
        get_filename_component(filename ${i} NAME)
        add_test( NAME Test_qa_stats_${filename}
            COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats ${i}" "." Test_qa_stats_${filename}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        if( TARGET qa-stats-tsan )
            add_test( NAME Test_qa_stats-tsan_${filename}
                COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats-tsan --multithreaded 1 ${i}" "." Test_qa_stats_${filename}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
    endforeach()

    # fingerprinting
    add_test( NAME Test_qa_stats_Finger_1
        COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats input/1.1.fastq --fingerprint" "." Test_qa_stats_Finger_1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET qa-stats-tsan )
        add_test( NAME Test_qa_stats-tsan_Finger_1
            COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats-tsan --multithreaded 1 input/1.1.fastq --fingerprint" "." Test_qa_stats_Finger_1
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    add_test( NAME Test_qa_stats_Finger_2
        COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats input/001.R1.fastq -f" "." Test_qa_stats_Finger_2
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET qa-stats-tsan )
        add_test( NAME Test_qa_stats-tsan_Finger_2
            COMMAND "./runtestcase.sh" "${BINDIR}/qa-stats-tsan --multithreaded 1 input/001.R1.fastq -f" "." Test_qa_stats_Finger_2
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    
endif()
