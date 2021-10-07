add_compile_definitions( __mod__="test/sra-sort" )

if ( NOT WIN32 )
    # this test requires env var TEST_DATA to be set, or it will return an error (1)
	add_test( NAME SlowTest_Sra_sort
		COMMAND test-sra-sort.sh ${BINDIR}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()