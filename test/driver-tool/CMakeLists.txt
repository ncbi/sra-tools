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

	# NO_SDL
	add_test( NAME Test_NO_SDL
		COMMAND no_sdl.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# bogus
	add_test( NAME Test_Bogus
		COMMAND bogus.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Testing
		COMMAND testing.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Vdbcache
		COMMAND vdbcache.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Split_3
		COMMAND split_3.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Split_e
		COMMAND split_e.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasta_missing_param
		COMMAND fasta_missing_param.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasta_default
		COMMAND fasta_default.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasta_0
		COMMAND fasta_0.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasta_number
		COMMAND fasta_number.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Fasta_neg_number
		COMMAND fasta_neg_number.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Help_srapath
		COMMAND help_srapath.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Help_prefetch
		COMMAND help_prefetch.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Help_fasterq-dump
		COMMAND help_fasterq-dump.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Help_fastq-dump
		COMMAND help_fastq-dump.sh "${CMAKE_BINARY_DIR}/bin" 
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()