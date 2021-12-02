add_compile_definitions( __mod__="test/vdb-config" )

if ( NOT WIN32 )
	add_executable( test-vdb-config-model test-vdb-config-model )
	target_link_libraries( test-vdb-config-model ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )

	add_test( NAME Test_VDB_Config_model
		COMMAND model.sh ${TESTBINDIR}/test-vdb-config-model
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_VDB_Config
		COMMAND test-vdb-config.sh "${BINDIR}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()