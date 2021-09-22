add_compile_definitions( __mod__="test/scripts" )

if ( NOT WIN32 )
	add_test( NAME Test_Scripts_prepare-sra-names
		COMMAND perl test-prepare-sra-names.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Scripts_fix-sra-names
		COMMAND perl test-fix-sra-names.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Scripts_prepare-sra-names-py
		COMMAND perl test-prepare-sra-names-py.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Scripts_fix-sra-names-py
		COMMAND perl test-fix-sra-names-py.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()