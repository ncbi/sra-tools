add_compile_definitions( __mod__="test/prefetch" )

# if( Python3_EXECUTABLE )
	# add_test( NAME Test_Vdb_dump_Check_exit_code
		# COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${BINDIR}/vdb-dump
		# WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
# endif()

if ( NOT WIN32 )
	add_executable( test-quality test-quality )
	target_link_libraries( test-quality ${COMMON_LINK_LIBRARIES} ncbi-vdb )

	add_test( NAME SlowTest_Prefetch
		COMMAND runtests.sh "${CMAKE_BINARY_DIR}/test-bin/test-quality" "${BINDIR}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()