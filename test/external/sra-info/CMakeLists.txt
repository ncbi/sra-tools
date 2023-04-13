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

add_compile_definitions( __mod__="test/external/sra-info" )

# unit tests

AddExecutableTest( Test_SraInfo_Unit "test-sra-info"
                                "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}"
                                "${PROJECT_SOURCE_DIR}/tools/external/sra-info" )

# for the benefit of the command line tests below, make sure to build the tool
add_dependencies( Test_SraInfo_Unit sra-info )

if(EXISTS "${DIRTOTEST}/sra-info${EXE}")

    make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

    # make sure 'sra-info -h' does not crash
    add_test( NAME Test_Sra_Info_Help
        COMMAND sh -c "${DIRTOTEST}/sra-info -h >actual/Test_Sra_Info_Help.stdout"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # fails without parameters
    add_test( NAME Test_Sra_Info_NoParam
        COMMAND sh -c "${DIRTOTEST}/sra-info 2>actual/Test_Sra_Info_NoParam.stderr"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_Sra_Info_NoParam PROPERTIES WILL_FAIL TRUE)
    # fails with more than 1 parameter
    add_test( NAME Test_Sra_Info_MultipleParams
        COMMAND sh -c "${DIRTOTEST}/sra-info SRR000123 SRR0001234 2>actual/Test_Sra_Info_MultipleParams.stderr"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_Sra_Info_MultipleParams PROPERTIES WILL_FAIL TRUE)

    # platform
    add_test( NAME Test_Sra_Info_Platform_Table_1.0
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P SRR000123" "." "1.0"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_CSRA_1.1
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ERR334733" "." "1.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_WGS_1.2
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P AAAAAA01" "." "1.2"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_Multiple_2.0
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ./input/MultiPlatform" "." "2.0"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_Reference_3.0
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P NC_000001.11" "." "3.0"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # formatting
    add_test( NAME Test_Sra_Info_Platform_Format_CSV_4.1
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ./input/MultiPlatform -f csv" "." "4.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_Format_XML_4.2
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ./input/MultiPlatform -f xml" "." "4.2"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_Format_Json_4.3
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ./input/MultiPlatform -f json" "." "4.3"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Platform_Format_Tab.4.4
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -P ./input/MultiPlatform -f tab" "." "4.4"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # is-aligned
    add_test( NAME Test_Sra_Info_IsAligned_No_5.1
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -A SRR000123" "." "5.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_IsAligned_Yes_5.2
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --is-aligned ERR334733" "." "5.2"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # quality
    add_test( NAME Test_Sra_Info_Quality_Physical_6.1
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --quality SRR000123" "." "6.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_Quality_Physical_6.2
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info -Q NC_000001.10" "." "6.2"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # spot layout
    add_test( NAME Test_Sra_Info_SpotLayout_7.1
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout SRR000123" "." "7.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_SpotLayout_Json_7.2
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout -f json SRR000123" "." "7.2"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_SpotLayout_CSV_7.3
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout -f csv SRR000123" "." "7.3"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_SpotLayout_Tab_7.4
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout -f tab SRR000123" "." "7.4"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Sra_Info_SpotLayout_XML_7.5
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout -f xml SRR000123" "." "7.5"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # TODO: find PACBIO and GC runs with CONSENSUS tables
    add_test( NAME Test_Sra_Info_SpotLayout_Pacbio_7.6
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout DRR032985 --limit 100" "." "7.6"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    # TODO: find a smaller CG run
    add_test( NAME SlowTest_Sra_Info_SpotLayout_CG_7.7
        COMMAND "./runtestcase.sh" "${BINDIR}/sra-info --spot-layout SRR546160 --limit 100" "." "7.7"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # # multiple queries
    # add_test( NAME Test_Sra_Info_MultipleQueries_8.0
    # COMMAND "./runtestcase.sh" "${BINDIR}/sra-info SRR000123 -P -A -Q -S" "." "8.0"
    # WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # # the default set of queries
    # add_test( NAME Test_Sra_Info_Default.8.0
    #     COMMAND "./runtestcase.sh" "${BINDIR}/sra-info SRR000123" "." "9.0"
    #     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
    message(WARNING "${DIRTOTEST}/sra-info${EXE} is not found. The corresponding tests are skipped." )
endif()
