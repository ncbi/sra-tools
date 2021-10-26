add_compile_definitions( __mod__="test/copycat" )

if ( ${OS} STREQUAL "linux" AND EXISTS ${BINDIR}/magic )

    add_test( NAME Test_Copycat_Help
        COMMAND ${BINDIR}/copycat -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        
    add_test( NAME Test_Copycat_Bad_Name 
        COMMAND ./test-bad-name.sh ${BINDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_Copycat_IdenticalCopy
        COMMAND ${BINDIR}/copycat
                ./input/1.xml actual/ && diff ./input/1.xml actual/1.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()

