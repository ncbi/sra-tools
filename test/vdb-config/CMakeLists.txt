add_compile_definitions( __mod__="test/vdb-config" )

if ( NOT WIN32 )
	add_executable( test-vdb-config-model test-vdb-config-model )
	target_link_libraries( test-vdb-config-model ${COMMON_LINK_LIBRARIES} ncbi-vdb )

	add_test( NAME Test_VDB_Config_model
		COMMAND model.sh ${CMAKE_BINARY_DIR}/test-bin/test-vdb-config-model
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_VDB_Config
		COMMAND test-vdb-config.sh "${CMAKE_BINARY_DIR}/bin"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()