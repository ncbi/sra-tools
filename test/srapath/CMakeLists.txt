add_compile_definitions( __mod__="test/srapath" )

if( Python3_EXECUTABLE )
    add_test( NAME Test_Srapath_Check_exit_code
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=..
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/srapath${EXE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()


if ( NOT WIN32 )
    add_test( NAME Test_Srapath
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=..
            bash -c "./test-srapath.sh ${DIRTOTEST}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()