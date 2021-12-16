if ( NOT WIN32 )
	add_test( NAME Test_Vdb_validate
		COMMAND vdb_validate.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()