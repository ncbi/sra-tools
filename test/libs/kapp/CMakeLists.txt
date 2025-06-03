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

if( WIN32 )
    set( ADDITIONAL_LIBS Crypt32 Ws2_32 )
else()
    set( ADDITIONAL_LIBS "" )
endif()

GenerateExecutableWithDefs( Test_KAPP_args "kapp-test" "" "" "ktst;kapp;${COMMON_LIBS_WRITE};${ADDITIONAL_LIBS}" )
if( WIN32 )
    # will be testing wide character path conversion; use wmain()
    target_link_options( Test_KAPP_args PRIVATE "/ENTRY:wmainCRTStartup" )
endif()
add_test( NAME Test_KAPP_args COMMAND Test_KAPP_args WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

if ( NOT WIN32 )

    GenerateExecutableWithDefs( sig-core-kapp "sig-core-kapp.c" "" "" "kapp;${COMMON_LIBS_READ}" )

    if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
        set( BLD dbg)
    else()
        set( BLD rel)
    endif()

    add_test( NAME CoreGenerationKapp COMMAND sh -c "./run-sig-core.sh ${CMAKE_TEST_OUTPUT_DIRECTORY}/sig-core-kapp ${OS} ${BLD}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()