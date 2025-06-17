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

add_compile_definitions( __mod__="test/vdb-diff" )

ToolsRequired( vdb-diff vdb-copy )

if( Python3_EXECUTABLE )
    if( WIN32 )
	  add_test( NAME Test_VDB_Diff_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-diff${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    else()
	  add_test( NAME Test_VDB_Diff_Check_exit_code
		COMMAND sh -c "ls ${DIRTOTEST}/vdb-diff${EXE}>/dev/null 2>&1 && ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-diff${EXE} || echo no vdb-diff"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
	if( TARGET vdb-diff-asan )
		add_test( NAME Test_VDB_Diff_Check_exit_code-asan
			COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-diff-asan${EXE}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET vdb-diff-tsan )
		add_test( NAME Test_VDB_Diff_Check_exit_code-tsan
			COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-diff-tsan${EXE}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
endif()

if( NOT WIN32 )
	set( ACCESSION SRR000001 )
	add_test( NAME Test_VDB_Diff_Check_success
		COMMAND sh test_success.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET vdb-diff-asan )
		add_test( NAME Test_VDB_Diff_Check_success-asan
			COMMAND sh test_success.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET vdb-diff-tsan )
		add_test( NAME Test_VDB_Diff_Check_success-tsan
			COMMAND sh test_success.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	add_test( NAME Test_VDB_Diff_Check_failure
		COMMAND sh test_failure.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET vdb-diff-asan )
		add_test( NAME Test_VDB_Diff_Check_failure-asan
			COMMAND sh test_failure.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET vdb-diff-tsan )
		add_test( NAME Test_VDB_Diff_Check_failure-tsan
			COMMAND sh test_failure.sh "${DIRTOTEST}" ${ACCESSION} vdb-diff-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
endif()
