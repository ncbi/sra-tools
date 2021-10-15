add_compile_definitions( __mod__="test/sra-sort" )

if ( NOT WIN32 )
    # if directory /export/home/TMP does not exist, the script will not run and exit with 0
    # if it does exist, the script requires env var TEST_DATA to be set, or it will return an error (1)
    # TEST_DATA is supposed to point to a directory with the inputs for this test
	add_test( NAME SlowTest_Sra_sort
		COMMAND test-sra-sort.sh ${BINDIR}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()