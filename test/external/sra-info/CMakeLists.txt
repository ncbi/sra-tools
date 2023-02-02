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

AddExecutableTest( Test_SraInfo "test-sra-info" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "${PROJECT_SOURCE_DIR}/tools/external/sra-info" )

if(EXISTS "${DIRTOTEST}/sra-info${EXE}")

    # make sure 'sra-info -h' does not crash
    add_test( NAME Test_Sra_Info_Help
        COMMAND ${DIRTOTEST}/sra-info -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # fails without parameters
    add_test( NAME Test_Sra_Info_NoParam
        COMMAND ${DIRTOTEST}/sra-info
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_Sra_Info_NoParam PROPERTIES WILL_FAIL TRUE)
    # fails with more than 1 parameter
    add_test( NAME Test_Sra_Info_MultipleParams
        COMMAND ${DIRTOTEST}/sra-info SRR000123 SRR0001234
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_Sra_Info_MultipleParams PROPERTIES WILL_FAIL TRUE)

else()
    message(WARNING "${DIRTOTEST}/sra-info${EXE} is not found. The corresponding tests are skipped." )
endif()
