add_compile_definitions( __mod__="test/vdb-copy" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_VDB_Copy_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-copy${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()
