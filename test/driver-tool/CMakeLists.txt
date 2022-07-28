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

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/driver-tool)

AddExecutableTest( Test_Drivertool_JsonParsing "test-json-parse.cpp" "" "")
AddExecutableTest( Test_Drivertool_SDLResponse "test-sdl-response.cpp" "" "")
AddExecutableTest( Test_Drivertool_BuildVersion "test-build-version.cpp" "" "")

if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
if ( WIN32 )
    add_executable( Drivertool_Win32_Args_Parsing_Test win32-cmdline-test.cpp )
    add_test( NAME Test_Drivertool_Win32_Args_Parsing COMMAND Drivertool_Win32_Args_Parsing_Test )
else ()
	# GOOD
	function( DriverToolGoodTest test_name tool_name )
		add_test( NAME ${test_name}
			COMMAND good_test.sh ${tool_name} "${DIRTOTEST}" sratools
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		if( RUN_SANITIZER_TESTS )
			add_test( NAME ${test_name}-asan
				COMMAND good_test.sh ${tool_name} "${DIRTOTEST}" sratools-asan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
			add_test( NAME ${test_name}-tsan
				COMMAND good_test.sh ${tool_name} "${DIRTOTEST}" sratools-tsan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		endif()
	endfunction()

	DriverToolGoodTest( Test_Drivertool_Fastq_dump fastq-dump )
	DriverToolGoodTest( Test_Drivertool_Fasterq_dump fasterq-dump )
	DriverToolGoodTest( Test_Drivertool_Sam_dump sam-dump )
	DriverToolGoodTest( Test_Drivertool_Sra_pileup sra-pileup )
	DriverToolGoodTest( Test_Drivertool_Vdb_dump vdb-dump )
	DriverToolGoodTest( Test_Drivertool_Prefetch prefetch )
	DriverToolGoodTest( Test_Drivertool_Srapath srapath )

	# CONTAINER
	function( DriverToolContainerTest acc )
		add_test( NAME Test_Drivertool_Container_${acc}
			COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		if( RUN_SANITIZER_TESTS )
			add_test( NAME Test_Drivertool_Container_${acc}-asan
				COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools-asan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
			add_test( NAME Test_Drivertool_Container_${acc}-tsan
				COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools-tsan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		endif()
	endfunction()

	DriverToolContainerTest( SRP000001 )
	DriverToolContainerTest( SRX000001 )
	DriverToolContainerTest( SRS000001 )
	DriverToolContainerTest( SRA000001 )
	DriverToolContainerTest( ERP000001 )
	DriverToolContainerTest( DRX000001 )

	# NO_SDL
	function( DriverToolTestNoScriptParams script_name )
		add_test( NAME Test_Drivertool_${script_name}
			COMMAND ${script_name}.sh "${DIRTOTEST}" sratools
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		if( RUN_SANITIZER_TESTS )
			add_test( NAME Test_Drivertool_${script_name}-asan
				COMMAND ${script_name}.sh "${DIRTOTEST}" sratools-asan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
			add_test( NAME Test_Drivertool_${script_name}-tsan
				COMMAND ${script_name}.sh "${DIRTOTEST}" sratools-tsan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		endif()
	endfunction()

	DriverToolTestNoScriptParams( no_sdl )

	# bogus
	DriverToolTestNoScriptParams( bogus )
	DriverToolTestNoScriptParams( testing )
	DriverToolTestNoScriptParams( vdbcache )
	DriverToolTestNoScriptParams( split_3 )
	DriverToolTestNoScriptParams( split_e )
	DriverToolTestNoScriptParams( fasta_missing_param )
	DriverToolTestNoScriptParams( fasta_default )
	DriverToolTestNoScriptParams( fasta_0 )
	DriverToolTestNoScriptParams( fasta_number )
	DriverToolTestNoScriptParams( fasta_neg_number )
	DriverToolTestNoScriptParams( help_srapath )
	DriverToolTestNoScriptParams( help_prefetch )
	DriverToolTestNoScriptParams( help_fasterq-dump )
	DriverToolTestNoScriptParams( help_fastq-dump )

	function( DriverToolTestScript2Params script_name p1 p2 )
		add_test( NAME Test_Drivertool_${script_name}_${p1}
			COMMAND ${script_name}.sh "${DIRTOTEST}" "${p1}" "${p2}" sratools
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		if( RUN_SANITIZER_TESTS )
			add_test( NAME Test_Drivertool_${script_name}_${p1}-asan
				COMMAND ${script_name}.sh "${DIRTOTEST}" "${p1}" "${p2}" sratools-asan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
			add_test( NAME Test_Drivertool_${script_name}_${p1}-tsan
				COMMAND ${script_name}.sh "${DIRTOTEST}" "${p1}" "${p2}" sratools-tsan
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		endif()
	endfunction()

	DriverToolTestScript2Params( fasta_er no_fasta_er "" )
	DriverToolTestScript2Params( fasta_er "fasta_er" "--fasta" )
	DriverToolTestScript2Params( fasta_er "fasta_er_unsorted" "--fasta-unsorted" )

	add_test( NAME Test_Drivertool_2_accessions
		COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( RUN_SANITIZER_TESTS )
		add_test( NAME Test_Drivertool_2_accessions-asan
			COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
		add_test( NAME Test_Drivertool_2_accessions-tsan
			COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

endif()
endif()
