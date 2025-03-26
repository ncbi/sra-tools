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

if( SINGLE_CONFIG )

    find_package(ZLIB)
    find_package(BZip2)
    find_package (Threads)
message(RE2_FOUND=${RE2_FOUND})
    if ( ZLIB_FOUND AND BZip2_FOUND AND Threads_FOUND AND RE2_FOUND )

        set(CMAKE_CXX_STANDARD 17)
        #specific to BitMagic
        if (COMPILER_OPTION_SSE42_SUPPORTED)
            add_definitions(-msse4.2 -DBMSSE42OPT)
        endif()

        # assume tools/sharq built libre2
        #TODO make it like in ./ngs/ngs-sdk
        if( SINGLE_CONFIG OR WIN32 )
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
        else() # XCode
            # TODO: test on XCode
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}) # Temporary solution
            #set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG})
            #set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE})
        endif()

        set(LOCAL_BUILD_DIR ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
        set(LOCAL_INCDIR ${LOCAL_BUILD_DIR}/include)
        set(LOCAL_LIBDIR ${LOCAL_BUILD_DIR}/lib)
        set(RE2_STATIC_LIBRARIES -L${LOCAL_LIBDIR} -lre2 )

        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
        endif()

        # test-regexpr
        add_executable(test-regexpr test-regexpr.cpp )
        add_dependencies(test-regexpr RE2)
        target_include_directories(test-regexpr PUBLIC ${LOCAL_INCDIR})
        target_link_libraries(test-regexpr ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${RE2_STATIC_LIBRARIES})
        add_test( NAME Test_regexpr COMMAND test-regexpr WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        # test-sharq-reader
        add_executable(test-sharq-reader test-sharq-reader.cpp )
        add_dependencies(test-sharq-reader RE2 sharq)
        target_include_directories(test-sharq-reader PUBLIC ${LOCAL_INCDIR} ../../../tools/loaders/sharq)
        target_link_libraries(test-sharq-reader ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
        add_test( NAME Test_sharq_reader COMMAND test-sharq-reader WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        # test-sharq-parser
        add_executable(test-sharq-parser test-sharq-parser.cpp )
        add_dependencies(test-sharq-parser RE2 sharq)
        target_include_directories(test-sharq-parser PUBLIC ${LOCAL_INCDIR} ../../../tools/loaders/sharq)
        target_link_libraries(test-sharq-parser ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
        add_test( NAME Test_sharq_parser COMMAND test-sharq-parser WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        # test-sharq-writer
        add_executable(test-sharq-writer test-sharq-writer.cpp )
        add_dependencies(test-sharq-writer RE2 sharq general-loader ) # general-loader for the metadata command line tests 
        target_include_directories(test-sharq-writer PUBLIC ${LOCAL_INCDIR} ../../../tools/loaders/sharq)
        target_link_libraries(test-sharq-writer ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ})
        add_test( NAME Test_sharq_writer COMMAND test-sharq-writer WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        if( TARGET sharq-asan )
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
                set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
            endif()

            # asan
            set( asan_defs "-fsanitize=address" )

            add_executable(test-sharq-parser-asan test-sharq-parser.cpp )
            add_dependencies(test-sharq-parser-asan RE2 sharq-asan)
            target_include_directories(test-sharq-parser-asan PUBLIC ${LOCAL_INCDIR} ../../../tools/loaders/sharq)
            target_compile_options( test-sharq-parser-asan PRIVATE ${asan_defs} )
            target_link_libraries(test-sharq-parser-asan ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
            target_link_options( test-sharq-parser-asan PRIVATE ${asan_defs} )
            add_test( NAME Test_sharq_parser_asan COMMAND test-sharq-parser-asan WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            #TODO: Test_regexpr_asan Test_sharq_writer_asan
        endif()

        if( TARGET sharq-tsan )
            if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
                set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
            endif()

            # tsan
            set( tsan_defs "-fsanitize=thread" )

            add_executable(test-sharq-parser-tsan test-sharq-parser.cpp )
            add_dependencies(test-sharq-parser-tsan RE2 sharq-tsan)
            target_include_directories(test-sharq-parser-tsan PUBLIC ${LOCAL_INCDIR} ../../../tools/loaders/sharq)
            target_compile_options( test-sharq-parser-tsan PRIVATE ${tsan_defs} )
            target_link_libraries(test-sharq-parser-tsan ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
            target_link_options( test-sharq-parser-tsan PRIVATE ${tsan_defs} )
            add_test( NAME Test_sharq_parser_tsan COMMAND test-sharq-parser-tsan WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
            #TODO: Test_regexpr_tsan Test_sharq_writer_tsan

        endif()

        #
        # command line tests
        #

        macro(SharqTestImpl TestName Result TestParams SharqBinary CaseId)
            add_test(
                NAME Test_${TestName}
                COMMAND
                    "./runtestcase.sh" "${DIRTOTEST}" ${SharqBinary} "." "${CaseId}" "${Result}" "0" "--debug" "--no-timestamp" "${TestParams}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        endmacro()

        macro(SharqTestNormal TestName Result TestParams)
            SharqTestImpl(${TestName} ${Result} ${TestParams} "sharq" ${TestName})
        endmacro()

        if( RUN_SANITIZER_TESTS )
            macro(SharqTestAsan TestName Result TestParams)
                SharqTestImpl("${TestName}-asan" ${Result} ${TestParams} "sharq-asan" ${TestName})
            endmacro()
            macro(SharqTestTsan TestName Result TestParams)
                SharqTestImpl("${TestName}-tsan" ${Result} ${TestParams} "sharq-tsan" ${TestName})
            endmacro()
        endif()

        macro(SharqTest TestName Result TestParams)
            unset(spot_assembly)
            set (extra_args ${ARGN})
            # Did we get any optional args?
            list(LENGTH extra_args extra_count)
            if (${extra_count} GREATER 0)
                list(GET extra_args 0 spot_assembly)
            endif ()

            if (DEFINED spot_assembly AND spot_assembly STREQUAL "only-sa")
                set(params "--spot-assembly ${TestParams}")
                SharqTestImpl("${TestName}-sa-hot" ${Result} ${params} "sharq" ${TestName}-sa-hot)

                set(params "--spot-assembly --hot-reads-threshold 0 ${TestParams}")
                SharqTestImpl("${TestName}-sa-cold" ${Result} ${params} "sharq" ${TestName}-sa-cold)
            else()
                SharqTestNormal(${TestName} ${Result} ${TestParams})
                if (NOT DEFINED spot_assembly OR NOT spot_assembly STREQUAL "no-sa")
                    set(params "--spot-assembly ${TestParams}")
                    SharqTestImpl("${TestName}-sa-hot" ${Result} ${params} "sharq" ${TestName}-sa-hot)

                    set(params "--spot-assembly --hot-reads-threshold 0 ${TestParams}")
                    SharqTestImpl("${TestName}-sa-cold" ${Result} ${params} "sharq" ${TestName}-sa-cold)
                endif()
            endif()
            if( RUN_SANITIZER_TESTS )
                SharqTestAsan(${TestName} ${Result} ${TestParams})
                SharqTestTsan(${TestName} ${Result} ${TestParams})
            endif()
        endmacro()

        macro(SharqTestNoSA TestName Result TestParams)
            SharqTestNormal(${TestName} ${Result} ${TestParams})
            if( RUN_SANITIZER_TESTS )
                SharqTestAsan(${TestName} ${Result} ${TestParams})
                SharqTestTsan(${TestName} ${Result} ${TestParams})
            endif()
        endmacro()


        macro(SharqTestWithTelemetryImpl TestName Result TestParams SharqBinary CaseId)
        add_test( NAME Test_${TestName}
            COMMAND
                "./runtestcase.sh" "${DIRTOTEST}" ${SharqBinary} "." "${CaseId}" "${Result}" "1" "--debug" "--no-timestamp" "${TestParams}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        endmacro()
        macro(SharqTestWithTelemetryNormal TestName Result TestParams)
            SharqTestWithTelemetryImpl(${TestName} ${Result} ${TestParams} "sharq" ${TestName})
        endmacro()
        if( RUN_SANITIZER_TESTS )
            macro(SharqTestWithTelemetryAsan TestName Result TestParams)
                SharqTestWithTelemetryImpl("${TestName}-asan" ${Result} ${TestParams} "sharq-asan" ${TestName})
            endmacro()
            macro(SharqTestWithTelemetryTsan TestName Result TestParams)
                SharqTestWithTelemetryImpl("${TestName}-tsan" ${Result} ${TestParams} "sharq-tsan" ${TestName})
            endmacro()
        endif()
        macro(SharqTestWithTelemetry TestName Result TestParams)
            SharqTestWithTelemetryNormal(${TestName} ${Result} ${TestParams})
            if( RUN_SANITIZER_TESTS )
                SharqTestWithTelemetryAsan(${TestName} ${Result} ${TestParams})
                SharqTestWithTelemetryTsan(${TestName} ${Result} ${TestParams})
            endif()
        endmacro()

        #set(IN "${CMAKE_CURRENT_SOURCE_DIR}/input/")
        set(IN "input/")
        SharqTest(001.read.supported 0 "${IN}001.R1.fastq")
        SharqTest(001.read.unsupported 1 "--max-err-count 0 ${IN}001.read.unsupported.fq")
        SharqTest(001.read.no_sequence 1 "--max-err-count 0 ${IN}001.read.no_sequence.fq")
        SharqTest(001.read.invalid_sequence 1 "--max-err-count 0 ${IN}001.read.invalid_sequence.fq")
        SharqTest(001.read.no_quality 1 "--max-err-count 0 ${IN}001.read.no_quality.fq")
        SharqTest(001.read.no_quality_2 1 "--max-err-count 0 ${IN}001.read.no_quality_2.fq")
        SharqTest(001.read.invalid_quality33 1 "--max-err-count 0 --quality=33 ${IN}001.read.invalid_quality33.fq")
        SharqTest(001.read.invalid_quality64 1 "--max-err-count 0 --quality=64 ${IN}001.read.invalid_quality64.fq")
        SharqTest(001.read.invalid_quality0 1 "--max-err-count 0 --quality=0 ${IN}001.read.invalid_quality0.fq")
        SharqTest(001.read.long_quality 0 "--max-err-count 0 ${IN}001.read.long_quality.fq") # only a warning
        SharqTest(001.read.bad_quality0 1 "--max-err-count 0 ${IN}001.read.bad_quality0.fq")
        SharqTest(001.read.bad_quality33 1 "--max-err-count 0 ${IN}001.read.bad_quality33.fq")
        SharqTest(002.readTypes_default 0 "${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
        SharqTest(002.readTypes_technical 0 "--readTypes=TT ${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
        SharqTest(002.readTypes_mixed 0 "--readTypes=TB ${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
        SharqTest(002.readTypes_missing 1 "--read1PairFiles=${IN}001.R1.fastq")
        SharqTest(002.readTypes_mismatch 1 "--readTypes=TB --read1PairFiles=${IN}001.R1.fastq")
        SharqTest(002.readTypes_unexpected 1 "--readTypes=X --read1PairFiles=${IN}002.allBiological_1.fq")
        SharqTest(002.readTypes_interlaved_fail_1 1 "${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq" "no-sa")
        SharqTest(002.readTypes_interleaved_fail_4 1 "--readTypes=BB ${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq" "no-sa")
        SharqTest(002.readTypes_interlaved_fail_3 1 "${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq" "only-sa")
        SharqTest(002.readTypes_interlaved_fail_2 1 "--readTypes=BB ${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq" "only-sa")
        SharqTest(002.readTypes_interlaved_pass 0 "--readTypes=BTB ${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq")
        SharqTest(003.files_not_exists 1 "dummy1.fq")
        SharqTest(003.files_not_valid 1 "${IN}003.empty.fq")
        SharqTest(003.files_readpairs 0 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq,${IN}003.t3_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
        SharqTest(003.files_mixed 1 "${IN}005.offset0_1.fq ${IN}001.R1.fastq")
        SharqTest(003.files_mixed_cluster 1 "${IN}003.t_R1.fastq ${IN}003.t_I1.fastq ${IN}001.R1.fastq" "no-sa")
        SharqTest(003.files_10x_mixed 1 "${IN}003.t_I1.fastq ${IN}003.t_R1.fastq ${IN}003.t_R2.fastq ${IN}003._I1.fastq ${IN}003.R1.fastq ${IN}003.R2.fastq")
        SharqTest(003.readTypes_10x 0 "${IN}003.t_R1.fastq ${IN}003.t_R1.fastq ${IN}003.t_I1.fastq" "no-sa")
        SharqTest(003.wrong_pairing 1 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
        SharqTest(004.truncated.fail 1 "${IN}004.truncated_1.fq.gz ${IN}004.truncated_2.fq.gz" "no-sa")
        SharqTest(004.truncated.pass 0 "--allowEarlyFileEnd ${IN}004.truncated_1.fq.gz ${IN}004.truncated_2.fq.gz")
        SharqTest(005.offset0 0 "${IN}005.offset0_1.fq ${IN}005.offset0_2.fq")
        SharqTest(005.offset64 0 "${IN}005.offset64_1.fq ${IN}005.offset64_2.fq")
        SharqTestNoSA(006.duplicate 1 "--max-err-count 0 ${IN}006.duplicate.fq")
        SharqTest(007.digest.udenfined 0 "--digest ${IN}001.read.unsupported.fq")
        SharqTest(007.digest.multiple 0 "--digest ${IN}003.t_R1.fastq ${IN}003.t_R1.fastq ${IN}003.t_I1.fastq")
        SharqTest(007.digest.groups 0 "--digest ${IN}003.t_R1.fastq.gz ${IN}003.t2_R1.fastq ${IN}003.t3_R1.fastq ${IN}003.t_R2.fastq.gz ${IN}003.t2_R2.fastq ${IN}003.t3_R2.fastq ${IN}003.t_I1.fastq.gz ${IN}003.t2_I1.fastq ${IN}003.t3_I1.fastq" "no-sa")
        SharqTest(007.digest.error 0 "--max-err-count 0 --digest ${IN}001.read.invalid_quality0.fq")
        SharqTestWithTelemetry(008.telemetry_pass  0 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq,${IN}003.t3_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
        SharqTestNoSA(009.more_than_4_per_spot 1 "--debug ${IN}009.test_I1_001.fastq ${IN}009.test_R1_001.fastq ${IN}009.test_R2_001.fastq ${IN}009.test_R3_001.fastq")
        SharqTest(010.errors.invalid_seq_pass1 0 "${IN}010.errors.invalid_seq.fq")
        SharqTest(010.errors.invalid_seq_pass2 1 "--max-err-count 3 ${IN}010.errors.invalid_seq.fq")
        SharqTest(010.errors.invalid_seq_fail 1 "--max-err-count 3 ${IN}010.errors.invalid_seq.fq")

        SharqTest(011.nanopore.3 0 "--max-err-count 0 ${IN}011.nanopore.3.fq")
        SharqTest(012.qualities 0 "--max-err-count 0 ${IN}012.test_I1_001.fastq ${IN}012.test_R1_001.fastq ${IN}012.test_R2_001.fastq ${IN}012.test2_I1_001.fastq ${IN}012.test2_R1_001.fastq ${IN}012.test2_R2_001.fastq") #VDB-4745
        SharqTest(012.qualities_with_types 0 "--readTypes=TTB ${IN}012.test_I1_001.fastq ${IN}012.test_R1_001.fastq ${IN}012.test_R2_001.fastq ${IN}012.test2_I1_001.fastq ${IN}012.test2_R1_001.fastq ${IN}012.test2_R2_001.fastq" "only-sa") #VDB-4745
        SharqTest(013.no-reads 0 "--max-err-count 0 ${IN}013.t_I1.fastq ${IN}013.t_I2.fastq ${IN}013.t_R1.fastq ${IN}013.t_R2.fastq") #VDB-4745
        SharqTest(014.2files_bad_sequence 0 "${IN}014.R1.fastq ${IN}014.R2.fastq" "no-sa")
        SharqTest(015.pending_spots 0 "${IN}015.R1.fq ${IN}015.R2.fq") # SRAO-632
        SharqTestWithTelemetry(016.set.platform 0 "--platform=BGISEQ ${IN}001.R1.fastq")  # SRAO-657

        ToolsRequired( sharq general-loader kdbmeta )
        macro(SharqTestWithMetadata TestName LoadParams MetaParams)
            add_test( NAME Test_${TestName}
                COMMAND
                    "./runtestcase-meta.sh" "${DIRTOTEST}/sharq" "${DIRTOTEST}/general-loader" "${DIRTOTEST}/kdbmeta" "." "${TestName}" "${LoadParams}" "${MetaParams}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
        endmacro()
        SharqTestWithMetadata(DbMetaSingleFile_input "${IN}001.R1.fastq" ". LOAD")
        SharqTestWithMetadata(DbMetaSingleFile_output "${IN}001.R1.fastq" "tbl/SEQUENCE QC/current")
        SharqTestWithMetadata(DbMetaMultipleFiles_input "${IN}r1.fastq ${IN}r2.fastq" ". LOAD")
        SharqTestWithMetadata(DbMetaMultipleFiles_output "${IN}r1.fastq ${IN}r2.fastq" "tbl/SEQUENCE QC")

    endif()

endif()
