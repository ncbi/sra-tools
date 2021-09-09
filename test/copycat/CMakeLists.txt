add_compile_definitions( __mod__="test/align-info" )

if ( NOT WIN32 )

    add_test( NAME Test_Copycat_Help
        COMMAND ${CMAKE_BINARY_DIR}/bin/copycat -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    # Fails:
    # copycat.2.11.1 int: libs/kfs/ffmagic.c:943:KMagicFileFormatMake: loadable library unexpected while loading file format within file format detection module - failed to create file format
    # add_test( NAME Test_Copycat_IdenticalCopy
    #     COMMAND ${CMAKE_BINARY_DIR}/bin/copycat
    #             ./input/1.xml actual/ && diff ./input/1.xml actual/1.xml
    #     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()

