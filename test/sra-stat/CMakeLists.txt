add_compile_definitions( __mod__="test/sra-stat" )

if ( NOT WIN32 )
	add_executable( testAssemblyStatistics testAssemblyStatistics )
	target_link_libraries( testAssemblyStatistics ${COMMON_LINK_LIBRARIES} ncbi-vdb )

	add_test( NAME Test_Sra-stat
		COMMAND test-sra-stat.sh ${CMAKE_BINARY_DIR}/bin
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME SlowTest_Sra-stat
		COMMAND test-sra-stat-slow.sh ${CMAKE_BINARY_DIR}/bin
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()