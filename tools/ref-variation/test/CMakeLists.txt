if ( NOT WIN32)
	add_test ( NAME Ref-Variation-1.0 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./have-access.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ref-variation )
	add_test ( NAME Var-Expand-1.0    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./have-access.sh ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} var-expand )
endif()
