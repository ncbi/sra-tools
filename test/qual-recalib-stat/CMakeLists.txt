add_compile_definitions( __mod__="test/qual-recalib-stat" )

set( dir "CMAKE_RUNTIME_OUTPUT_DIRECTORY$<$<CONFIG:Debug>:_DEBUG>$<$<CONFIG:Release>:_RELEASE>" )
if( Python3_EXECUTABLE )
	add_test( NAME Test_QualRecalibStat_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py
			${dir}/qual-recalib-stat
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND_EXPAND_LISTS
	)

endif()
