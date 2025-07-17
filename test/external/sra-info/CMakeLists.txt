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
                                "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}"
                                "${PROJECT_SOURCE_DIR}/tools/external/sra-info" )

ToolsRequired(sra-info)

make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

# make sure 'sra-info -h' does not crash
if ( WIN32 )
  add_test( NAME Test_Sra_Info_Help
    COMMAND ${DIRTOTEST}/sra-info -h >actual/Test_Sra_Info_Help.stdout
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else ()
  add_test( NAME Test_Sra_Info_Help
    COMMAND sh -c "ls ${DIRTOTEST}/sra-info>/dev/null 2>&1 && ${DIRTOTEST}/sra-info -h >actual/Test_Sra_Info_Help.stdout || echo no sra-info"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

### command line checking:
if ( WIN32 )
  function( AddCmdlineFailureTest test_name )
    add_test( NAME ${test_name}
        COMMAND ${DIRTOTEST}/sra-info ${ARGN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
  endfunction()
else ()
  function( AddCmdlineFailureTest test_name args )
    add_test( NAME ${test_name}
        COMMAND "./runfailtest.sh" "${DIRTOTEST}/sra-info" "${args}" "." ${test_id}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
  endfunction()
endif()

# fails without parameters
if ( WIN32 )
    AddCmdlineFailureTest( Test_Sra_Info_NoParam )
else ()
    AddCmdlineFailureTest( Test_Sra_Info_NoParam "" )
endif()

if ( WIN32 )
    # fails with more than 1 parameter
    AddCmdlineFailureTest( Test_Sra_Info_MultipleParams SRR000123 SRR0001234 )
    # --limit has to be positive
    AddCmdlineFailureTest( Test_Sra_Info_Limit_Negative --spot-layout DRR032985 --limit -1 )
    AddCmdlineFailureTest( Test_Sra_Info_Limit_Zero --spot-layout DRR032985 --limit 0 )
else ()
    # fails with more than 1 parameter
    AddCmdlineFailureTest( Test_Sra_Info_MultipleParams "SRR000123 SRR0001234" )
    # --limit has to be positive
    AddCmdlineFailureTest( Test_Sra_Info_Limit_Negative "--spot-layout DRR032985 --limit -1" )
    AddCmdlineFailureTest( Test_Sra_Info_Limit_Zero "--spot-layout DRR032985 --limit 0" )
endif()
if ( NOT WIN32 )
## queries:
function( AddQueryTest test_name test_id args )
    add_test( NAME ${test_name}
        COMMAND "./runtestcase.sh" "${DIRTOTEST}/sra-info ${args}" "." ${test_id}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 9000)
endfunction()

# platform
AddQueryTest( Test_Sra_Info_Platform_Table      1.0 "-P SRR000123" )
AddQueryTest( Test_Sra_Info_Platform_CSRA       1.1 "-P ERR334733" )
AddQueryTest( Test_Sra_Info_Platform_WGS        1.2 "-P AAAAAA01" )
AddQueryTest( Test_Sra_Info_Platform_Multiple   2.0 "-P ./input/MultiPlatform_Long.sra"  )
AddQueryTest( Test_Sra_Info_Platform_Reference  3.0 "-P NC_000001.11" )
AddQueryTest( Test_Sra_Info_Platform_Missing    4.0 "-P SRR576646" )

# formatting
AddQueryTest( Test_Sra_Info_Platform_Format_CSV     4.1 "-P ./input/MultiPlatform.sra -f csv" )
AddQueryTest( Test_Sra_Info_Platform_Format_XML     4.2 "-P ./input/MultiPlatform.sra -f xml" )
AddQueryTest( Test_Sra_Info_Platform_Format_Json    4.3 "-P ./input/MultiPlatform.sra -f json" )
AddQueryTest( Test_Sra_Info_Platform_Format_Tab     4.4 "-P ./input/MultiPlatform.sra -f tab" )

# is-aligned
AddQueryTest( Test_Sra_Info_IsAligned_No    5.1 "-A SRR000123" )
AddQueryTest( Test_Sra_Info_IsAligned_Yes   5.2 "--is-aligned ERR334733" )

# quality
AddQueryTest( Test_Sra_Info_Quality_Physical        6.1 "--quality ./input/MultiPlatform.sra" )
AddQueryTest( Test_Sra_Info_Quality_Physical_Refseq 6.2 "-Q NC_000001.10" )
AddQueryTest( Test_Sra_Info_Quality_delite          6.3 "-Q SRR000123" )

# spot layout
AddQueryTest( Test_Sra_Info_SpotLayout_Default  7.1 "--spot-layout SRR000123" )
AddQueryTest( Test_Sra_Info_SpotLayout_Json     7.2 "--spot-layout -f json SRR000123" )
AddQueryTest( Test_Sra_Info_SpotLayout_CSV      7.3 "--spot-layout -f csv SRR000123" )
AddQueryTest( Test_Sra_Info_SpotLayout_Tab      7.4 "--spot-layout -f tab SRR000123" )
AddQueryTest( Test_Sra_Info_SpotLayout_XML      7.5 "--spot-layout -f xml SRR000123" )

# PACBIO, uses the CONSENSUS table
AddQueryTest( Test_Sra_Info_SpotLayout_Pacbio_Consensus 7.6.1 "--spot-layout DRR032985 --limit 100" )
# PACBIO, uses the SEQUENCE table
AddQueryTest( Test_Sra_Info_SpotLayout_Pacbio_Sequence 7.6.2 "--spot-layout DRR032985 --limit 100 --sequence" )

# TODO: find a smaller CG run
AddQueryTest( SlowTest_Sra_Info_SpotLayout_CG 7.7 "--spot-layout SRR546160 --limit 100" )

# spot layout, detail level
AddQueryTest( Test_Sra_Info_SpotLayout_Detail_Short         7.8.1 "--spot-layout SRR000123 --detail 0" )
AddQueryTest( Test_Sra_Info_SpotLayout_Detail_Abbreviated   7.8.2 "--spot-layout SRR000123 --detail 1" )
AddQueryTest( Test_Sra_Info_SpotLayout_Detail_Full          7.8.3 "--spot-layout SRR000123 --detail 2" )

# spot layout, first N rows only
AddQueryTest( Test_Sra_Info_SpotLayout_Rows          7.9 "--spot-layout SRR000123 --rows 5" )

# # multiple queries
# add_test( NAME Test_Sra_Info_MultipleQueries_8.0
# COMMAND "./runtestcase.sh" "${DIRTOTEST}/sra-info SRR000123 -P -A -Q -S" "." "8.0"
# WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# # the default set of queries
# add_test( NAME Test_Sra_Info_Default.8.0
#     COMMAND "./runtestcase.sh" "${DIRTOTEST}/sra-info SRR000123" "." "9.0"
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# schema, Default format
AddQueryTest( Test_Sra_Info_Schema_Default 9.0 "--schema input/MultiPlatform.sra" )

# schema, JSON format
AddQueryTest( Test_Sra_Info_Schema_Json 9.1 "--schema input/MultiPlatform.sra -fjson" )

# schema, XML format
AddQueryTest( Test_Sra_Info_Schema_Xml 9.2 "--schema input/MultiPlatform.sra -fxml" )

# query for ACPQS, Default format
AddQueryTest( Test_Sra_Info_ACPQS_Default 10.0 "input/MultiPlatform.sra -ACPQS" )

# query for ACPQS, JSON format
AddQueryTest( Test_Sra_Info_ACPQS_Json 10.1 "input/MultiPlatform.sra -ACPQS -fjson" )

# query for ACPQS, XML format
AddQueryTest( Test_Sra_Info_ACPQS_Xml 10.2 "input/MultiPlatform.sra -ACPQS -fxml" )

# query for contents
AddQueryTest( Test_Sra_Info_Contents_Table 11.0 "-T SRR000123" )

AddQueryTest( Test_Sra_Info_Contents_DB_Default 11.1 "--contents SRR341576" )
AddQueryTest( Test_Sra_Info_Contents_DB_Json 11.2 "--contents SRR341576 -fjson" )

AddQueryTest( Test_Sra_Info_Contents_Detail_Short_Default 12.0 "--contents SRR000123 --detail 0" )
AddQueryTest( Test_Sra_Info_Contents_Detail_Short_Json 12.1 "--contents SRR341576 -fjson --detail 0" )
AddQueryTest( Test_Sra_Info_Contents_Detail_Detailed_Default 12.2 "--contents SRR341576 --detail 1" )
AddQueryTest( Test_Sra_Info_Contents_Detail_Detailed_Json 12.3 "--contents SRR341576 -fjson --detail 1" )
AddQueryTest( Test_Sra_Info_Contents_Detail_Verbose_Default 12.4 "--contents SRR341576 --detail 2" )
AddQueryTest( Test_Sra_Info_Contents_Detail_Verbose_Json 12.5 "--contents SRR341576 -fjson --detail 3" )

# query for fingerprinting info
AddQueryTest( Test_Sra_Info_Fingerprint_None 13.0 "--fingerprint input/Test_Sra_Info_Fingerprint_None.sra" )
AddQueryTest( Test_Sra_Info_Fingerprint_Short 13.1 "--fingerprint input/Test_Sra_Info_Fingerprint" )
AddQueryTest( Test_Sra_Info_Fingerprint_Abbreviated 13.2 "--fingerprint input/Test_Sra_Info_Fingerprint --detail 1" )
AddQueryTest( Test_Sra_Info_Fingerprint_Detailed 13.3 "--fingerprint input/Test_Sra_Info_Fingerprint --detail 2" )
AddQueryTest( Test_Sra_Info_Fingerprint_Detailed_NotFound 13.4 "--fingerprint SRR000123 --detail 2" )

AddQueryTest( Test_Sra_Info_Fingerprint_Csv_None 14.0 "--fingerprint input/Test_Sra_Info_Fingerprint_None.sra --format csv" )
AddQueryTest( Test_Sra_Info_Fingerprint_Csv_Short 14.1 "--fingerprint input/Test_Sra_Info_Fingerprint --format csv" )
AddQueryTest( Test_Sra_Info_Fingerprint_Csv_Abbreviated 14.2 "--fingerprint input/Test_Sra_Info_Fingerprint --format csv --detail 1" )
AddQueryTest( Test_Sra_Info_Fingerprint_Csv_Detailed 14.3 "--fingerprint input/Test_Sra_Info_Fingerprint --format csv --detail 2" )

AddQueryTest( Test_Sra_Info_Fingerprint_Json_None 15.0 "--fingerprint input/Test_Sra_Info_Fingerprint_None.sra --format json" )
AddQueryTest( Test_Sra_Info_Fingerprint_Json_Short 15.1 "--fingerprint input/Test_Sra_Info_Fingerprint --format json" )
AddQueryTest( Test_Sra_Info_Fingerprint_Json_Abbreviated 15.2 "--fingerprint input/Test_Sra_Info_Fingerprint --format json --detail 1" )
AddQueryTest( Test_Sra_Info_Fingerprint_Json_Detailed 15.3 "--fingerprint input/Test_Sra_Info_Fingerprint --format json --detail 2" )

AddQueryTest( Test_Sra_Info_Fingerprint_Xml_None 16.0 "--fingerprint input/Test_Sra_Info_Fingerprint_None.sra --format xml" )
AddQueryTest( Test_Sra_Info_Fingerprint_Xml_Short 16.1 "--fingerprint input/Test_Sra_Info_Fingerprint --format xml" )
AddQueryTest( Test_Sra_Info_Fingerprint_Xml_Abbreviated 16.2 "--fingerprint input/Test_Sra_Info_Fingerprint --format xml --detail 1" )
AddQueryTest( Test_Sra_Info_Fingerprint_Xml_Detailed 16.3 "--fingerprint input/Test_Sra_Info_Fingerprint --format xml --detail 2" )

endif()
