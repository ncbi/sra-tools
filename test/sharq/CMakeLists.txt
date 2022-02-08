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

if ( ZLIB_FOUND AND BZip2_FOUND AND Threads_FOUND )

    set(CMAKE_CXX_STANDARD 17)
    add_definitions(-msse4.2 -DBMSSE42OPT)

    set(LOCAL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs)
    set(LOCAL_INCDIR ${LOCAL_BUILD_DIR}/include)
    set(LOCAL_LIBDIR ${LOCAL_BUILD_DIR}/lib)

    # Google RE2 release
    set(RE2_TAG 2021-09-01)

    include (ExternalProject)
    ExternalProject_Add(RE2_
    PREFIX RE2
    GIT_REPOSITORY https://github.com/google/re2.git
    GIT_TAG ${RE2_TAG}
    BUILD_IN_SOURCE 1
    UPDATE_COMMAND "" # No update as we use specific tag
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DRE2_BUILD_TESTING:BOOL=OFF
        -DCMAKE_INSTALL_PREFIX:PATH=${LOCAL_BUILD_DIR}
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
        -DCMAKE_INSTALL_LIBDIR:PATH=lib
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
        -DCMAKE_INSTALL_MESSAGE:STRING=LAZY
    )
    set(RE2_STATIC_LIBRARIES -L${LOCAL_LIBDIR} -lre2 )
    add_executable(test-sharq-parser test-sharq-parser.cpp )
    add_dependencies(test-sharq-parser RE2_ sharq)
    target_include_directories(test-sharq-parser PUBLIC ${LOCAL_INCDIR} ../../tools/sharq)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
    endif()
    target_link_libraries(test-sharq-parser ${CXX_FILESYSTEM_LIBRARIES} ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

    add_test( NAME Test_sharq_parser COMMAND test-sharq-parser WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    macro(SharqTest TestName Result TestParams)
    add_test( NAME Test_${TestName}
        COMMAND
            "./runtestcase.sh" "${BINDIR}" "." "${TestName}" "${Result}" "0" "--debug" "--no-timestamp" "${TestParams}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    endmacro()

    macro(SharqTestWithTelemetry TestName Result TestParams)
    add_test( NAME Test_${TestName}
        COMMAND
            "./runtestcase.sh" "${BINDIR}" "." "${TestName}" "${Result}" "1" "--debug" "--no-timestamp" "${TestParams}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
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
    SharqTest(001.read.invalid_quality64 1 "--quality=64 ${IN}001.read.invalid_quality64.fq")
    SharqTest(001.read.invalid_quality0 1 "--max-err-count 0 --quality=0 ${IN}001.read.invalid_quality0.fq")
    SharqTest(001.read.long_quality 1 "--max-err-count 0 ${IN}001.read.long_quality.fq")
    SharqTest(001.read.bad_quality0 1 "--max-err-count 0 ${IN}001.read.bad_quality0.fq")
    SharqTest(001.read.bad_quality33 1 "--max-err-count 0 ${IN}001.read.bad_quality33.fq")
    SharqTest(002.readTypes_default 0 "${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
    SharqTest(002.readTypes_technical 0 "--readTypes=TT ${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
    SharqTest(002.readTypes_mixed 0 "--readTypes=TB ${IN}002.allBiological_1.fq ${IN}002.allBiological_2.fq")
    SharqTest(002.readTypes_missing 1 "--read1PairFiles=${IN}001.R1.fastq")
    SharqTest(002.readTypes_mismatch 1 "--readTypes=TB --read1PairFiles=${IN}001.R1.fastq")
    SharqTest(002.readTypes_unexpected 1 "--readTypes=X --read1PairFiles=${IN}002.allBiological_1.fq")
    SharqTest(002.readTypes_interlaved_fail_1 1 "${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq")
    SharqTest(002.readTypes_interlaved_fail_2 1 "--readTypes=BB ${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq")
    SharqTest(002.readTypes_interlaved_pass 0 "--readTypes=BTB ${IN}002.readTypes_interleaved_1.fq ${IN}002.readTypes_interleaved_2.fq")
    SharqTest(003.files_not_exists 1 "dummy1.fq")
    SharqTest(003.files_not_valid 1 "${IN}003.empty.fq")
    SharqTest(003.files_readpairs 0 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq,${IN}003.t3_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
    SharqTest(003.files_mixed 1 "${IN}005.offset0_1.fq ${IN}001.R1.fastq")
    SharqTest(003.files_mixed_cluster 1 "${IN}003.t_R1.fastq ${IN}003.t_I1.fastq ${IN}001.R1.fastq")
    SharqTest(003.files_10x_mixed 1 "${IN}003.t_I1.fastq ${IN}003.t_R1.fastq ${IN}003.t_R2.fastq ${IN}003._I1.fastq ${IN}003.R1.fastq ${IN}003.R2.fastq")
    SharqTest(003.readTypes_10x 0 "${IN}003.t_R1.fastq ${IN}003.t_R1.fastq ${IN}003.t_I1.fastq")
    SharqTest(003.wrong_pairing 1 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
    SharqTest(004.truncated.fail 1 "${IN}004.truncated_1.fq.gz ${IN}004.truncated_2.fq.gz")
    SharqTest(004.truncated.pass 0 "--allowEarlyFileEnd ${IN}004.truncated_1.fq.gz ${IN}004.truncated_2.fq.gz")
    SharqTest(005.offset0 0 "${IN}005.offset0_1.fq ${IN}005.offset0_2.fq")
    SharqTest(005.offset64 0 "${IN}005.offset64_1.fq ${IN}005.offset64_2.fq")
    SharqTest(006.duplicate 1 "${IN}006.duplicate.fq")
    SharqTest(007.digest.udenfined 0 "--digest ${IN}001.read.unsupported.fq")
    SharqTest(007.digest.multiple 0 "--digest ${IN}003.t_R1.fastq ${IN}003.t_R1.fastq ${IN}003.t_I1.fastq")
    SharqTest(007.digest.groups 0 "--digest ${IN}003.t_R1.fastq.gz ${IN}003.t2_R1.fastq ${IN}003.t3_R1.fastq ${IN}003.t_R2.fastq.gz ${IN}003.t2_R2.fastq ${IN}003.t3_R2.fastq ${IN}003.t_I1.fastq.gz ${IN}003.t2_I1.fastq ${IN}003.t3_I1.fastq")
    SharqTest(007.digest.error 0 "--max-err-count 0 --digest ${IN}001.read.invalid_quality0.fq")
    SharqTestWithTelemetry(008.telemetry_pass  0 "--readTypes=BBT --read1PairFiles=${IN}003.t_R1.fastq.gz,${IN}003.t2_R1.fastq,${IN}003.t3_R1.fastq --read2PairFiles=${IN}003.t_R2.fastq.gz,${IN}003.t2_R2.fastq,${IN}003.t3_R2.fastq --read3PairFiles=${IN}003.t_I1.fastq.gz,${IN}003.t2_I1.fastq,${IN}003.t3_I1.fastq")
    SharqTest(009.more_than_4_per_spot 1 "--debug ${IN}009.test_I1_001.fastq ${IN}009.test_R1_001.fastq ${IN}009.test_R2_001.fastq ${IN}009.test_R3_001.fastq")
    SharqTest(010.errors.invalid_seq_pass1 0 "${IN}010.errors.invalid_seq.fq")
    SharqTest(010.errors.invalid_seq_pass2 0 "--max-err-count 5 ${IN}010.errors.invalid_seq.fq")
    SharqTest(010.errors.invalid_seq_fail 1 "--max-err-count 3 ${IN}010.errors.invalid_seq.fq")

    SharqTest(011.nanopore.3 0 "--max-err-count 0 ${IN}011.nanopore.3.fq")

    endif()

endif()