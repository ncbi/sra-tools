add_compile_definitions( __mod__="test/vdb-loader" )

if ( NOT WIN32 )
	add_executable( test-vcf-loader test-vcf-loader )
	target_link_libraries( test-vcf-loader ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} vcfloader )

	add_test( NAME Test_Vcf_loader
		COMMAND vcf-loader.sh ${TESTBINDIR}/test-vdb-config-model "${VDB_SRCDIR}/interfaces"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()