add_compile_definitions( __mod__="test/vdb-dump" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_Vdb_dump_Check_exit_code
		COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=..
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-dump${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND_EXPAND_LISTS
	)
endif()

if ( NOT WIN32 )

add_executable( vdb-dump-makedb makedb )
	target_link_libraries( vdb-dump-makedb ${COMMON_LINK_LIBRARIES} ncbi-wvdb )

	add_test( NAME Test_Vdb_dump
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=..
            bash -c "./vdb_dump.sh ${TESTBINDIR}/vdb-dump-makedb ${DIRTOTEST}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()