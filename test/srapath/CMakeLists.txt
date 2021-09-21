add_compile_definitions( __mod__="test/srapath" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_Srapath_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${CMAKE_BINARY_DIR}/bin/srapath
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()


if ( NOT WIN32 )
	add_test( NAME Test_Srapath
		COMMAND test-srapath.sh "${CMAKE_BINARY_DIR}/bin"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()