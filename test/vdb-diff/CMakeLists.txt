add_compile_definitions( __mod__="test/vdb-diff" )

set( ACCESSION SRR000001 )

if( Python3_EXECUTABLE )
	add_test( NAME Test_VDB_Diff_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-diff${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

if( NOT WIN32 )
	add_test( NAME Test_VDB_Diff_Check_success
		COMMAND test_success.sh "${DIRTOTEST}" ${ACCESSION}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_VDB_Diff_Check_failure
		COMMAND test_failure.sh "${DIRTOTEST}" ${ACCESSION}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()