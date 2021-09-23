if( Python3_EXECUTABLE )

    add_test( NAME SlowTest_NgsPython
        COMMAND ${Python3_EXECUTABLE} tests.py ${TestParams}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()