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

add_compile_definitions( __mod__="test/external/srapath" )

if ( EXISTS "${DIRTOTEST}/srapath${EXE}" )

if( Python3_EXECUTABLE )
    add_test( NAME Test_Srapath_Check_exit_code
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=../..
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/srapath${EXE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()


if ( NOT WIN32 )
    add_test( NAME Test_Srapath
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=../..
            bash -c "./test-srapath.sh ${DIRTOTEST} srapath"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( RUN_SANITIZER_TESTS )
        add_test( NAME Test_Srapath-asan
            COMMAND
                ${CMAKE_COMMAND} -E env VDB_CONFIG=../..
                bash -c "./test-srapath.sh ${DIRTOTEST} srapath-asan"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        add_test( NAME Test_Srapath-tsan
            COMMAND
                ${CMAKE_COMMAND} -E env VDB_CONFIG=../..
                bash -c "./test-srapath.sh ${DIRTOTEST} srapath-tsan"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
endif()

else()
	message(WARNING "${DIRTOTEST}/srapath${EXE} is not found. The corresponding tests are skipped." )
endif()
