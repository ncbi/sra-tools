add_compile_definitions( __mod__="test/qual-recalib-stat" )

if( Python3_EXECUTABLE )

	add_test( NAME Test_QualRecalibStat_Check_exit_code
		COMMAND
            ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/build/check-exit-code.py
                ${BINDIR}/qual-recalib-stat${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND_EXPAND_LISTS
	)

endif()
