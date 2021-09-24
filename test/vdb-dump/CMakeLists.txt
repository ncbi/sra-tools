add_compile_definitions( __mod__="test/vdb-dump" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_Vdb_dump_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${CMAKE_BINARY_DIR}/bin/vdb-dump
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

if ( NOT WIN32 )
	add_executable( vdb-dump-makedb makedb )
	target_link_libraries( vdb-dump-makedb ${COMMON_LINK_LIBRARIES} ncbi-wvdb )

	add_test( NAME Test_Vdb_dump
		COMMAND vdb_dump.sh "${CMAKE_BINARY_DIR}/test-bin/vdb-dump-makedb" "${CMAKE_BINARY_DIR}/bin"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()