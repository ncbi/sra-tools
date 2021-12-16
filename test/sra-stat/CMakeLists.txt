add_compile_definitions( __mod__="test/sra-stat" )

if ( NOT WIN32 )
	add_executable( testAssemblyStatistics testAssemblyStatistics )
	target_link_libraries( testAssemblyStatistics ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )

	add_test( NAME Test_Sra_stat
		COMMAND test-sra-stat.sh ${DIRTOTEST}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME SlowTest_Sra_stat
		COMMAND test-sra-stat-slow.sh ${DIRTOTEST}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()