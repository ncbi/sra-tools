add_compile_definitions( __mod__="test/align-info" )

if ( NOT WIN32 )
	# # make sure align-info -h does not crash
    add_test( NAME Test_Align_Info
          COMMAND ${BINDIR}/align-info -h
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else()
#TODO: make run on Windows
endif()

	# # make sure align-info -h does not crash
	# @ $(DIRTOTEST)/align-info -h > /dev/null
