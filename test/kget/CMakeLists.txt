if( Python3_EXECUTABLE )

if ( NOT WIN32 )

    add_test( NAME SlowTest_Kget
        COMMAND
        ${CMAKE_COMMAND} -E env PATH=${BINDIR}
        ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_COMMAND} -E env NCBI_VDB_RELIABLE=y
        ${Python3_EXECUTABLE} test_kget.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME SlowTest_Kget_DiffVsWget
        COMMAND diff-vs-wget.sh ${BINDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()

endif()