if ( NOT WIN32 )

	# GOOD
	add_test( NAME Test_Fastq_dump
		COMMAND good_test.sh fastq-dump "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasterq_dump
		COMMAND good_test.sh fasterq-dump "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Sam_dump
		COMMAND good_test.sh sam-dump "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Sra_pileup
		COMMAND good_test.sh sra-pileup "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Vdb_dump
		COMMAND good_test.sh vdb-dump "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Prefetch
		COMMAND good_test.sh prefetch "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Srapath
		COMMAND good_test.sh srapath "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# CONTAINER
	add_test( NAME Test_Container_SRP000001
		COMMAND container_test.sh SRP000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Container_SRX000001
		COMMAND container_test.sh SRX000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Container_SRS000001
		COMMAND container_test.sh SRS000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Container_SRA000001
		COMMAND container_test.sh SRA000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Container_ERP000001
		COMMAND container_test.sh ERP000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Container_DRX000001
		COMMAND container_test.sh DRX000001 "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()