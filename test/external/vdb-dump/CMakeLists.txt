# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

add_compile_definitions( __mod__="test/external/vdb-dump" )

if( Python3_EXECUTABLE )
	add_test( NAME Test_Vdb_dump_Check_exit_code
		COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=..
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-dump${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND_EXPAND_LISTS
	)
endif()

set(TOOL_HOME ${CMAKE_SOURCE_DIR}/tools/external/vdb-dump)

file ( MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/db )
AddExecutableTest( Test_Vdb_dump_view-spec
    "test-view-spec;${TOOL_HOME}/vdb-dump-view-spec.c;${TOOL_HOME}/vdb-dump-helper.c;${TOOL_HOME}/vdb-dump-context.c;${TOOL_HOME}/vdb-dump-coldefs.c;${TOOL_HOME}/vdb-dump-str.c"
    "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}"
    "${CMAKE_SOURCE_DIR}/tools/external/vdb-dump" )

if ( NOT WIN32 )

	add_executable( vdb-dump-makedb makedb )
	target_link_libraries( vdb-dump-makedb kapp ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
	add_dependencies(vdb-dump-makedb vdb-dump)
	if( TARGET vdb-dump-asan )
		add_dependencies(vdb-dump-makedb vdb-dump-asan)
	endif()
	if( TARGET vdb-dump-tsan )
		add_dependencies(vdb-dump-makedb vdb-dump-tsan)
	endif()

    add_test( NAME Test_Vdb_dump
        COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            sh -c "./vdb_dump.sh ${TESTBINDIR}/vdb-dump-makedb ${DIRTOTEST} vdb-dump"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

 	if( TARGET vdb-dump-asan )
         add_test( NAME Test_Vdb_dump_asan
             COMMAND
                 ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
                 sh -c "./vdb_dump.sh ${TESTBINDIR}/vdb-dump-makedb ${DIRTOTEST} vdb-dump-asan"
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         )
 	endif()
	if( TARGET vdb-dump-tsan )
        add_test( NAME Test_Vdb_dump_tsan
            COMMAND
                ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
                sh -c "./vdb_dump.sh ${TESTBINDIR}/vdb-dump-makedb ${DIRTOTEST} vdb-dump-tsan"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()

    # run this test after Test_Vdb_dump to make sure the test database has been created
    AddExecutableTest( Test_Vdb_dump_view-alias
        "test-view-alias"
        "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}"
        "${CMAKE_SOURCE_DIR}/tools/external/vdb-dump"
    )

endif()
