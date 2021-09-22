add_compile_definitions( __mod__="test/sra-sort" )

if ( NOT WIN32 )
	add_test( NAME Test_Sra_sort
		COMMAND test-sra-sort.sh ${CMAKE_BINARY_DIR}/bin
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()