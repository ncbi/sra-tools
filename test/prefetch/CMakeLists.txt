add_compile_definitions( __mod__="test/prefetch" )

# if( Python3_EXECUTABLE )
	# add_test( NAME Test_Vdb_dump_Check_exit_code
		# COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-dump
		# WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
# endif()

if ( NOT WIN32 )
	BuildExecutableForTest( SlowTest_Prefetch "test-quality" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

	add_test( NAME Test_Prefetch_urls_and_accs
		COMMAND perl urls_and_accs.pl ${BINDIR} ${BINDIR}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME SlowTest_Prefetch
		COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            bash -c "./runtests.sh ${CMAKE_BINARY_DIR}/test-bin/SlowTest_Prefetch ${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()
