if ( NOT WIN32 )
    add_test( NAME Test_Fasterq_dump
        COMMAND good_test.sh fasterq-dump "${CMAKE_BINARY_DIR}/bin" 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()