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

add_compile_definitions( __mod__="test/internal/read-filter-redact" )


if ( NOT WIN32 )
    ToolsRequired( read-filter-redact kar kdbmeta vdb-unlock )

    if( Python3_EXECUTABLE AND BUILD_TOOLS_LOADERS )

        add_test( NAME Test_Read_filter_redact_Check_exit_code
            COMMAND
sh -c "ls ${DIRTOTEST}/read-filter-redact${EXE}>/dev/null 2>&1 && ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/read-filter-redact${EXE} || echo no read-filter-redact"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND_EXPAND_LISTS
        )

    endif()

	make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

	add_test( NAME Test_Read_filter_redact_1
		COMMAND test-history.sh ${DIRTOTEST} read-filter-redact Test_Read_filter_redact_1
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Read_filter_redact_2
		COMMAND test-history.sh ${DIRTOTEST} read-filter-redact Test_Read_filter_redact_2
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    add_test( NAME Test_Read_filter_redact_3
		COMMAND test-history.sh ${DIRTOTEST} read-filter-redact Test_Read_filter_redact_3
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# 2 spots, each with one read
	add_test( NAME Test_Read_filter_redact_two_spots_qc
		COMMAND sh test-qc.sh ${DIRTOTEST} read-filter-redact 2spots
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# 2 spots, each with one reds, no loaded QC meta
	add_test( NAME Test_Read_filter_redact_no_qc
		COMMAND sh test-qc.sh ${DIRTOTEST} read-filter-redact no_qc NO_QC
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# 2 spots, each has 2 reads
	add_test( NAME Test_Read_filter_redact_two_reads_qc
		COMMAND sh test-qc.sh ${DIRTOTEST} read-filter-redact 2reads
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	if( TARGET read-filter-redact-asan )
		add_test( NAME Test_Read_filter_redact-asan
			COMMAND test-history.sh ${DIRTOTEST} read-filter-redact-asan Test_Read_filter_redact_1
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
		add_test( NAME Test_Read_filter_redact-tsan
			COMMAND test-history.sh ${DIRTOTEST} read-filter-redact-tsan Test_Read_filter_redact_1
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

endif()
