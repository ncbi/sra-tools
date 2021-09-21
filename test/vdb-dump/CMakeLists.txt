add_compile_definitions( __mod__="test/vdb-dump" )

if ( NOT WIN32 )

	add_executable( vdb-dump-makedb makedb )
	target_link_libraries( vdb-dump-makedb ${COMMON_LINK_LIBRARIES} ncbi-wvdb )

	add_test( NAME Test_Fastq_dump
		COMMAND vdb_dump.sh vdb-dump-makedb "${CMAKE_BINARY_DIR}/bin" ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()