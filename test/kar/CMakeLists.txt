if ( NOT WIN32 )

    add_test( NAME SlowTest_Kar
        COMMAND kar-ntest.sh ${CMAKE_BINARY_DIR}/bin/kar ${CMAKE_BINARY_DIR}/bin/prefetch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()
