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

set( SOURCEDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/external/driver-tool )

if (WIN32)
    add_compile_definitions(USE_WIDE_API=1)
    link_libraries(ShLwApi Pathcch)
    set( FILE_PATH_CPP "file-path.win32.cpp")
else()
    set( FILE_PATH_CPP "file-path.posix.cpp")
endif()

AddExecutableTest( Test_Drivertool_BuildVersion "test-build-version.cpp" "" "${SOURCEDIR}" )
AddExecutableTest( Test_Drivertool_FilePath "test-filepath.cpp" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "${SOURCEDIR}")
AddExecutableTest( Test_Drivertool_CommandLine "test-command-line.cpp" "" "${SOURCEDIR}")
AddExecutableTest( Test_Drivertool_JsonParsing "test-json-parse.cpp" "" "${SOURCEDIR}" )
AddExecutableTest( Test_Drivertool_SDLResponse "test-sdl-response.cpp" "" "${SOURCEDIR}" )
AddExecutableTest( Test_Drivertool_Accession "test-accession.cpp" "" "${SOURCEDIR}" )
AddExecutableTest( Test_Drivertool_UUID "test-uuid.cpp" "" "${SOURCEDIR}" )
AddExecutableTest( Test_Drivertool_RunSource "test-run-source.cpp;${SOURCEDIR}/run-source.cpp;${SOURCEDIR}/command-line.cpp;${SOURCEDIR}/SDL-response.cpp;${SOURCEDIR}/${FILE_PATH_CPP};${SOURCEDIR}/json-parse.cpp;${SOURCEDIR}/build-version.cpp;${SOURCEDIR}/tool-args.cpp;" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "${SOURCEDIR}")
AddExecutableTest( Test_Drivertool_ToolArgs "test-tool-args.cpp;${SOURCEDIR}/tool-args.cpp;${SOURCEDIR}/command-line.cpp;${SOURCEDIR}/build-version.cpp;${SOURCEDIR}/${FILE_PATH_CPP}" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" "${SOURCEDIR}")

if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    if ( WIN32 )
        add_executable( Drivertool_Win32_Args_Parsing_Test win32-cmdline-test.cpp )
        target_include_directories(Drivertool_Win32_Args_Parsing_Test PRIVATE "${SOURCEDIR}")
        add_test( NAME Test_Drivertool_Win32_Args_Parsing COMMAND Drivertool_Win32_Args_Parsing_Test )
    else ()

        function( DriverToolGoodTest_impl script_file test_name tool_name )
            add_test( NAME ${test_name}
                COMMAND ${script_file} ${tool_name} "${DIRTOTEST}" sratools
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            if( TARGET sratools-asan )
                add_test( NAME ${test_name}-asan
                    COMMAND ${script_file} ${tool_name} "${DIRTOTEST}" sratools-asan
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            endif()
            if( TARGET sratools-tsan )
                add_test( NAME ${test_name}-tsan
                    COMMAND ${script_file} ${tool_name} "${DIRTOTEST}" sratools-tsan
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            endif()
        endfunction()


        function( DriverToolGoodTest test_name tool_name )
            DriverToolGoodTest_impl( good_test.sh ${test_name} ${tool_name} )
            DriverToolGoodTest_impl( good_test_url.sh ${test_name}_url ${tool_name} )
        endfunction()

        function( DriverToolContainerTest acc )
            add_test( NAME Test_Drivertool_Container_${acc}
                COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            if( TARGET sratools-asan )
                add_test( NAME Test_Drivertool_Container_${acc}-asan
                    COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools-asan
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            endif()
            if( TARGET sratools-tsan )
                add_test( NAME Test_Drivertool_Container_${acc}-tsan
                    COMMAND container_test.sh ${acc} "${DIRTOTEST}" sratools-tsan
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            endif()
        endfunction()

        function( DriverToolTestNoScriptParams script_name )
            add_test( NAME Test_Drivertool_${script_name}
                COMMAND ${script_name}.sh "${DIRTOTEST}" sratools
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            if( TARGET sratools-asan )
                add_test( NAME Test_Drivertool_${script_name}-asan
                    COMMAND ${script_name}.sh "${DIRTOTEST}" sratools-asan
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            endif()
            if( TARGET sratools-tsan )
                add_test( NAME Test_Drivertool_${script_name}-tsan
                    COMMAND ${script_name}.sh "${DIRTOTEST}" sratools-tsan
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

        DriverToolContainerTest( SRP000001 )
        DriverToolContainerTest( SRX000001 )
        DriverToolContainerTest( SRS000001 )
        DriverToolContainerTest( SRA000001 )
        DriverToolContainerTest( ERP000001 )
        DriverToolContainerTest( DRX000001 )

        DriverToolTestNoScriptParams( no_sdl )
        DriverToolTestNoScriptParams( bogus )
        DriverToolTestNoScriptParams( test-path )
        DriverToolTestNoScriptParams( test-no-path )
        DriverToolTestNoScriptParams( test-opt-bitmap )
if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
        DriverToolTestNoScriptParams( test-opt-bitmap-Debug )
endif()
        DriverToolTestNoScriptParams( testing )
        DriverToolTestNoScriptParams( vdbcache )

        add_test( NAME Test_Drivertool_2_accessions
            COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        if( TARGET sratools-asan )
            add_test( NAME Test_Drivertool_2_accessions-asan
                COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools-asan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
        if( TARGET sratools-tsan )
            add_test( NAME Test_Drivertool_2_accessions-tsan
                COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions" sratools-tsan
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
        endif()
    endif()
endif()

