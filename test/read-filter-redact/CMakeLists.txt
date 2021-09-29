add_compile_definitions( __mod__="test/read-filter-redact" )

if( Python3_EXECUTABLE )
    add_test( NAME Test_Read_filter_redact_Check_exit_code
        COMMAND
            ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/build/check-exit-code.py
                ${BINDIR}/vdb-unlock${EXE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND_EXPAND_LISTS
    )
endif()

if ( NOT WIN32 )
	add_test( NAME Test_Read_filter_redact
		COMMAND runtests.sh ${BINDIR}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()