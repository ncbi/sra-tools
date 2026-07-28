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
# ==============================================================================

add_compile_definitions( __mod__="test/external/prefetch" )

# if( Python3_EXECUTABLE )
	# add_test( NAME Test_Vdb_dump_Check_exit_code
		# COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-dump
		# WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
# endif()

if ( NOT WIN32 )
	ToolsRequired(prefetch srapath)

	GenerateExecutableWithDefs( test-quality "test-quality" "" "" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

	add_test( NAME Test_Prefetch_In_PATH
		COMMAND
			${CMAKE_COMMAND} -E env PATH=${DIRTOTEST}:/bin sh -c "prefetch -h > /dev/null"
	)

	add_test( NAME Test_Prefetch_urls_and_accs
		COMMAND perl urls_and_accs.pl ${DIRTOTEST} ${BINDIR} prefetch #VERBOSE
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
 	if( TARGET prefetch-asan )
 		add_test( NAME Test_Prefetch_urls_and_accs-asan
 			COMMAND perl urls_and_accs.pl ${DIRTOTEST} ${BINDIR} prefetch-asan
 			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
 	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME Test_Prefetch_urls_and_accs-tsan
			COMMAND perl urls_and_accs.pl ${DIRTOTEST} ${BINDIR} prefetch-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	add_test( NAME Test_Prefetch_out_dir_and_file
		COMMAND
               perl out_dir_and_file.pl ${DIRTOTEST} ${BINDIR} prefetch #VERBOSE
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	if( TARGET prefetch-asan )
		add_test( NAME Test_Prefetch_out_dir_and_file-asan
			COMMAND perl out_dir_and_file.pl ${DIRTOTEST} ${BINDIR} prefetch-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME Test_Prefetch_out_dir_and_file-tsan
			COMMAND perl out_dir_and_file.pl ${DIRTOTEST} ${BINDIR} prefetch-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	add_test( NAME SlowTest_Prefetch_dflt
		COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            sh runtests.sh ${DIRTOTEST} prefetch ${TESTBINDIR}  # 123
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET prefetch-asan )
		add_test( NAME SlowTest_Prefetch_dflt-asan
			COMMAND
				${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
				sh runtests.sh ${DIRTOTEST} prefetch-asan ${TESTBINDIR}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME SlowTest_Prefetch_dflt-tsan
			COMMAND
				${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
				sh runtests.sh ${DIRTOTEST} prefetch-tsan ${TESTBINDIR}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

    add_test( NAME Test_Prefetch_outfile
        COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            sh test-out-file.sh ${DIRTOTEST} prefetch # 123
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET prefetch-asan )
        add_test( NAME Test_Prefetch_outfile-asan
            COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            bash test-out-file.sh ${DIRTOTEST} prefetch-asan
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET prefetch-tsan )
        add_test( NAME Test_Prefetch_outfile-tsan
            COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
            sh test-out-file.sh ${DIRTOTEST} prefetch-tsan
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

	add_test( NAME Test_Prefetch_outs
		COMMAND perl test-prefetch-outs.pl ${DIRTOTEST} prefetch 0  # 1234
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET prefetch-asan )
		add_test( NAME Test_Prefetch_outs-asan
            COMMAND perl test-prefetch-outs.pl ${DIRTOTEST} prefetch-asan
		    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME Test_Prefetch_outs-tsan
            COMMAND perl test-prefetch-outs.pl ${DIRTOTEST} prefetch-tsan
		    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	add_test( NAME SlowTest_Prefetch_1GB
		COMMAND perl ncbi1GB.pl ${DIRTOTEST} prefetch  # 23
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET prefetch-asan )
		add_test( NAME SlowTest_Prefetch_1GB-asan
			COMMAND perl ncbi1GB.pl ${BINDIR} prefetch-asan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME SlowTest_Prefetch_1GB-tsan
			COMMAND perl ncbi1GB.pl ${BINDIR} prefetch-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

    set( BAD_KART_ENV dbgap|110600={\"\":404,\"result\":[{\"status\":200,\"files\":[{\"object\":\"dbgap|110600\",\"locations\":[{\"link\":\"https://www.ncbi.nlm.nih.gov/Traces/sdlr/sdlr.cgi?debug=not-found\"}]}]}]}
          dbgap|110601={\"\":500,\"result\":[{\"status\":200,\"files\":[{\"object\":\"dbgap|110601\",\"locations\":[{\"link\":\"https://trace.ncbi.nlm.nih.gov/Traces/sdlr/sdlr.cgi?jwt=e\"}]}]}]}
          dbgap|110602={\"\":404,\"result\":[{\"status\":200,\"files\":[{\"object\":\"dbgap|110602\",\"locations\":[{\"link\":\"https://www.ncbi.nlm.nih.gov/Traces/sdlr/sdlr.cgi?debug=not-found\"}]}]}]}
          dbgap|110603={\"\":400,\"result\":[{\"status\":200,\"files\":[{\"object\":\"dbgap|110603\",\"locations\":[{\"link\":\"https://trace.ncbi.nlm.nih.gov/Traces/sdlr/sdlr.cgi\"}]}]}]})
	add_test( NAME Test_Prefetch_Bad_Kart
        COMMAND
          env ${BAD_KART_ENV}
          ${CONFIGTOUSE}=/ ./test-bad-kart.sh ${DIRTOTEST} prefetch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET prefetch-asan )
        add_test( NAME Test_Prefetch_Bad_Kart-asan
        COMMAND
          env ${BAD_KART_ENV}
          ${CONFIGTOUSE}=/ ./test-bad-kart.sh ${DIRTOTEST} prefetch-asan
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
        add_test( NAME Test_Prefetch_Bad_Kart-tsan
        COMMAND
          env ${BAD_KART_ENV}
          ${CONFIGTOUSE}=/ ./test-bad-kart.sh ${DIRTOTEST} prefetch-tsan
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    add_test ( NAME Test_Prefetch_download_after_csra
        COMMAND env ${CONFIGTOUSE}=/ sh test-caching-disabled.sh ${DIRTOTEST} prefetch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET prefetch-asan )
        add_test( NAME Test_Prefetch_download_after_csra-asan
            COMMAND env ${CONFIGTOUSE}=/ sh test-caching-disabled.sh ${DIRTOTEST} prefetch
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()
    if( TARGET prefetch-tsan )
        add_test( NAME Test_Prefetch_download_after_csra-tsan
            COMMAND env ${CONFIGTOUSE}=/ sh test-caching-disabled.sh ${DIRTOTEST} prefetch
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    add_test( NAME SlowTest_Prefetch_vdbcache
        COMMAND perl vdbcache.pl ${DIRTOTEST} prefetch 0 # 23
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    if( TARGET prefetch-asan ) # see VDB-6064
        add_test( NAME SlowTest_Prefetch_vdbcache-asan
            COMMAND perl vdbcache.pl ${BINDIR} prefetch-asan
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET prefetch-tsan )
		add_test( NAME SlowTest_Prefetch_vdbcache-tsan
			COMMAND perl vdbcache.pl ${BINDIR} prefetch-tsan
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
endif()
