if ( NOT WIN32 )

    add_test( NAME Test_FasterqDump_Help
        COMMAND ${BINDIR}/fasterq-dump -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_FasterqDump_NotZeroWithoutParameters
        COMMAND ${BINDIR}/fasterq-dump
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(Test_FasterqDump_NotZeroWithoutParameters PROPERTIES WILL_FAIL TRUE)

else()
#TODO: make run on Windows
endif()
