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
sh -c "ls ${BINDIR}/read-filter-redact${EXE}>/dev/null 2>&1 && ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${BINDIR}/read-filter-redact${EXE} || echo no read-filter-redact"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND_EXPAND_LISTS
        )

	    if( TARGET read-filter-redact-asan )
		  add_test( NAME Test_Read_filter_redact_Check_exit_code-asan
            COMMAND
sh -c "ls ${BINDIR}/read-filter-redact${EXE}-asan>/dev/null 2>&1 && ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${BINDIR}/read-filter-redact${EXE}-asan || echo no read-filter-redact"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND_EXPAND_LISTS
          )
        endif()
	    if( TARGET read-filter-redact-tsan )
		  add_test( NAME Test_Read_filter_redact_Check_exit_code-tsan
            COMMAND
sh -c "ls ${BINDIR}/read-filter-redact${EXE}-tsan>/dev/null 2>&1 && ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${BINDIR}/read-filter-redact${EXE}-tsan || echo no read-filter-redact"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMAND_EXPAND_LISTS
          )
        endif()
    endif()

	make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

	add_test( NAME Test_Read_filter_redact_h1
		COMMAND test-history.sh ${BINDIR} read-filter-redact Test_Read_filter_redact_1
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_h1-asan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-asan Test_Read_filter_redact_1
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_h1-tsan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-tsan Test_Read_filter_redact_1
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

    add_test( NAME Test_Read_filter_redact_h2
		COMMAND test-history.sh ${BINDIR} read-filter-redact Test_Read_filter_redact_2
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_h2-asan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-asan Test_Read_filter_redact_2
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_h2-tsan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-tsan Test_Read_filter_redact_2
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

    add_test( NAME Test_Read_filter_redact_h3
		COMMAND test-history.sh ${BINDIR} read-filter-redact Test_Read_filter_redact_3
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_h3-asan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-asan Test_Read_filter_redact_3
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_h3-tsan
		COMMAND test-history.sh ${BINDIR} read-filter-redact-tsan Test_Read_filter_redact_3
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	# 2 spots, each with one read
	add_test( NAME Test_Read_filter_redact_qtwo_spots_qc
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact 2spots
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_qtwo_spots_qc-asan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-asan 2spots
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_qtwo_spots_qc-tsan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-tsan 2spots
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	# 2 spots, each with one reds, no loaded QC meta
	add_test( NAME Test_Read_filter_redact_qno_qc
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact no_qc NO_QC
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_qno_qc-asan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-asan no_qc NO_QC
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_qno_qc-tsan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-tsan no_qc NO_QC
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	# 2 spots, each has 2 reads
	add_test( NAME Test_Read_filter_redact_qtwo_reads_qc
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact 2reads
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
      add_test( NAME Test_Read_filter_redact_qtwo_reads_qc-asan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-asan 2reads
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
      add_test( NAME Test_Read_filter_redact_qtwo_reads_qc-tsan
		COMMAND sh test-qc.sh ${BINDIR} read-filter-redact-tsan 2reads
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	add_test( NAME Test_Read_filter_redact_u_quality
		COMMAND sh test-upd-qual.sh ${BINDIR} read-filter-redact
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET read-filter-redact-asan )
		add_test( NAME Test_Read_filter_redact_u_quality-asan
			COMMAND sh test-upd-qual.sh ${BINDIR} read-filter-redact-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET read-filter-redact-tsan )
		add_test( NAME Test_Read_filter_redact_u_quality-tsan
			COMMAND sh test-upd-qual.sh ${BINDIR} read-filter-redact-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
endif()
