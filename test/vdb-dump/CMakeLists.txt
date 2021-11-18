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

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    add_test(NAME VdbDumpSetup COMMAND bash -c "echo 'vdb/schema/paths = \"${VDB_SRCDIR}/interfaces\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-vdbdump\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(VdbDumpSetup PROPERTIES FIXTURES_SETUP VdbDumpTest)

    add_test( NAME Test_Vdb_dump
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=.
            bash -c "./vdb_dump.sh ${TESTBINDIR}/vdb-dump-makedb ${DIRTOTEST}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_Vdb_dump PROPERTIES FIXTURES_REQUIRED VdbDumpTest )

endif()