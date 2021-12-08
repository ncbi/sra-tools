if ( NOT WIN32 )

    add_test( NAME Test_Kar
        COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            bash -c "./kar-ntest.sh ${DIRTOTEST}/kar ${DIRTOTEST}/prefetch"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()
