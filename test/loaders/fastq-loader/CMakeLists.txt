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

if( NOT WIN32 )
    ToolsRequired(latf-load vdb-dump)

    # specify the location of schema files in a local .kfg file, to be used by the tests here as needed
    add_test(NAME LatfTestSetup COMMAND sh -c "echo 'vdb/schema/paths = \"${CMAKE_SOURCE_DIR}/libs/schema:${VDB_INCDIR}\"\n/LIBS/GUID=\"8test002-6ab7-41b2-bfd0-latfload\"' > tmp.kfg" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    set_tests_properties(LatfTestSetup PROPERTIES FIXTURES_SETUP LatfTest)

    set( LIBS )
    macro(LatfExeTest TestName Sources Libraries)
        AddExecutableTest( ${TestName} "${Sources}" "${Libraries}" "" )
        add_dependencies( ${TestName} latf-load )
    endmacro()

    if( NOT "bsd" STREQUAL ${OS} )
        LatfExeTest( SlowTest_FastqLoader_id2name     "test-id2name.cpp"          "kapp;fastqloader;loader;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
    endif()
    LatfExeTest( Test_FastqLoader_WbFastq_dflt    "wb-test-fastq.cpp"         "kapp;fastqloader;loader;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
    LatfExeTest( Test_FastqLoader_WbFastqParse    "wb-test-fastq-parse.cpp"   "kapp;fastqloader;loader;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

    LatfExeTest( Test_FastqLoader_WbFastqLoader   "test-fastq-loader.cpp"     "kapp;fastqloader;loader;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )
    set_tests_properties( Test_FastqLoader_WbFastqLoader
        PROPERTIES ENVIRONMENT "NCBI_SETTINGS=/;VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}"
    )

    #
    # test definition helpers
    #

    # basic test case, accepting parameters for the dumper and the loader
    macro(LatfTestInt TestName Result DumpParams LoadParams)
        add_test( NAME Test_FastqLoader_${TestName}
            COMMAND
                ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh runtestcase.sh "${DIRTOTEST}/vdb-dump ${DumpParams}" "${DIRTOTEST}/latf-load ${LoadParams}" "." "${TestName}" "${Result}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        set_tests_properties( Test_FastqLoader_${TestName} PROPERTIES FIXTURES_REQUIRED LatfTest )
        if(${Result} EQUAL "0" AND TARGET latf-load-asan )
            add_test( NAME Test_FastqLoader_${TestName}-asan
                COMMAND
                    ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                    ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                    sh runtestcase.sh "${BINDIR}/vdb-dump-asan ${DumpParams}" "${BINDIR}/latf-load-asan ${LoadParams}" "." "${TestName}" "${Result}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            set_tests_properties( Test_FastqLoader_${TestName}-asan PROPERTIES FIXTURES_REQUIRED LatfTest )
        endif()

        if(${Result} EQUAL "0" AND  TARGET latf-load-tsan )
            add_test( NAME Test_FastqLoader_${TestName}-tsan
                COMMAND
                    ${CMAKE_COMMAND} -E env NCBI_SETTINGS=/
                    ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                    sh runtestcase.sh "${BINDIR}/vdb-dump-tsan ${DumpParams}" "${BINDIR}/latf-load-tsan ${LoadParams}" "." "${TestName}" "${Result}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            set_tests_properties( Test_FastqLoader_${TestName}-tsan PROPERTIES FIXTURES_REQUIRED LatfTest )
        endif()
    endmacro()

    # default dump params
    macro(LatfTest TestName Result LoadParams)
        LatfTestInt( ${TestName} ${Result} "" "${LoadParams}" )
    endmacro()

    # default dump params; single input with the name made out of TestName
    macro(LatfTest_OneInput TestName Result LoadParams)
        LatfTestInt( ${TestName} ${Result} "" "${CMAKE_CURRENT_SOURCE_DIR}/input/${TestName}.fastq ${LoadParams}" )
    endmacro()

    # default dump params; single input with the most common --quality option
    macro(LatfTest_OneInputPhred33 TestName Result)
        LatfTest_OneInput( ${TestName} ${Result} "--quality PHRED_33" )
    endmacro()

    #
    # load-dump-diff test cases
    #

    #       Quality encoding: phred/logodds, ascii/decimal
    LatfTest_OneInputPhred33( 1.1 0 )
    LatfTest_OneInput( 1.2 0 "--quality PHRED_64")
    LatfTest_OneInputPhred33( 1.3 3 ) # no more decimal qualities
    LatfTest_OneInput( 1.4 0 "--quality LOGODDS")

    #       Single input file
    LatfTest_OneInputPhred33( 2.1 0 ) # reads only, basespace
#    LatfTest_OneInputPhred33( 2.1.1 3 ) # reads only, colorspace - no longer supported TODO: asserts
#    LatfTest_OneInputPhred33( 2.2 0 ) # qualities only TODO: fix or remove
#    LatfTest_OneInputPhred33( 2.3 0 ) # reads+qualities, qualities before reads TODO: fix or remove
#    LatfTest_OneInputPhred33( 2.4 0 ) # reads+qualities, reads before qualities TODO: fix or remove
    LatfTest_OneInputPhred33( 2.5 0 ) # reads+qualities, paired up
    LatfTest_OneInputPhred33( 2.6 0 ) # spot assembly
    LatfTest_OneInputPhred33( 2.7 3 ) # duplicate read names, fragments not specified - error
    LatfTest( 2.7.1 0  "input/2.7.fastq --quality PHRED_33 --allow_duplicates" ) # duplicate read names, fragments not specified - allowed with an option
#    LatfTest_OneInputPhred33( 2.8 0 ) # handling of N in reads, ALTREAD is a static column. TODO: update for the new schema; investigate intermittent failure (FIXED_SPOT_LEN == 0, not 3)
    LatfTest_OneInputPhred33( 2.8.1 0 ) # handling of N in reads, ALTREAD is a physical column
    LatfTest_OneInputPhred33( 2.9 3 ) # mismatching length of sequence and quality (qualities shorter)
    LatfTest_OneInputPhred33( 2.9.1 3 ) # mismatching length of sequence and quality (qualities longer)

    #       Multiple files
    LatfTest( 3.1 0 "input/3.1a.fastq input/3.1b.fastq --quality PHRED_33") # reads+qualities together
#    LatfTest( 3.2 0 "input/3.2a.fastq input/3.2b.fastq --quality PHRED_33") # qualities separately from reads, reads file comes first
#    LatfTest( 3.3 0 "input/3.3a.fastq input/3.3b.fastq --quality PHRED_33" ) # qualities separately from reads, qualities file comes first (reject?)
#    LatfTest( 3.4 0 "-1 input/3.4a.fastq -2 input/3.4b.fastq --quality PHRED_33" ) # default read number TODO: uncomment when VDB-4530 merges

    #   Command line options:
    #           4.1 --tmpfs          temporary files created at the specified location
    add_test( NAME Test_FastqLoader_4.1
        COMMAND
            ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
            sh -c "${DIRTOTEST}/latf-load ./input/4.fastq --tmpfs ./actual -v -o actual/4.1.obj --quality PHRED_33 2>&1 | grep \"Path for scratch files: ./actual/key2id\""
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if( TARGET latf-load-asan )
        add_test( NAME Test_FastqLoader_4.1-asan
            COMMAND
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/latf-load-asan ./input/4.fastq --tmpfs ./actual -v -o actual/4.1.obj --quality PHRED_33 2>&1 | grep \"Path for scratch files: ./actual/key2id\""
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
    if( TARGET latf-load-tsan )
        add_test( NAME Test_FastqLoader_4.1-tsan
            COMMAND
                ${CMAKE_COMMAND} -E env VDB_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}
                sh -c "${DIRTOTEST}/latf-load-tsan ./input/4.fastq --tmpfs ./actual -v -o actual/4.1.obj --quality PHRED_33 2>&1 | grep \"Path for scratch files: ./actual/key2id\""
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
    #           4.2 --qual-quant        quality values are quantized as specified
    LatfTest( 4.2 0 "input/4.fastq --qual-quant 1:10,10:20,20:30,30:- --quality PHRED_33" )
    #           4.3.1 --cache-size      rejected if invalid
    LatfTest( 4.3.1 3 "input/4.fastq --cache-size BIG --quality PHRED_33" )
    #           4.4 --max-rec-count     stops at the specified record #
#    LatfTest( 4.4 0 "input/4.4.fastq --max-rec-count 3 --quality PHRED_33" ) TODO: update for the new schema,
    #           4.5 --max-err-count     stops after the specified # of errors
    LatfTest( 4.5 3 "${CMAKE_CURRENT_SOURCE_DIR}/input/4.5.fastq --max-err-count 2 --quality PHRED_33" )
    #           4.5.1 --max-err-pct     fails if % of error records is too high
    #    LatfTest( 4.5.1 3 "${CMAKE_CURRENT_SOURCE_DIR}/input/4.5.1.fastq --quality PHRED_33 --max-err-count 10 --max-err-pct 20" ) TODO: adjust for extra output on stderr
    #           4.5.2 --max-err-count   VDB-4839: hangs after the specified # of errors
    LatfTest( 4.5.2 3 "${CMAKE_CURRENT_SOURCE_DIR}/input/4.5.2.fastq --platform ILLUMINA --quality PHRED_33 --max-err-count 2" )

#           4.6 --platform          saves correct value in the PLATFORM column
    LatfTest( 4.6 0 "input/4.fastq -p LS454 --quality PHRED_33" )
    #           4.6.1 --platform        rejects invalid values
    LatfTest( 4.6.1 3 "input/4.fastq -p SOMETHINGELSE --quality PHRED_33" )
    #           4.7 input file missing
    LatfTest( 4.7 3 "input/not_there --quality PHRED_33" )
    #           4.8 drop read names
    LatfTest( 4.8 0 "input/4.8.fastq --quality PHRED_33 --no-readnames" )
    #   Gzipped input
    LatfTest( 5.0 0 "input/5.0.fastq.gz --quality PHRED_33" )
    #   Misparsed quality
    LatfTest( 6.0 0 "input/6.0.fastq --quality PHRED_33" )

    #   PACBIO fastq
    LatfTest_OneInput( 7.1 0 "--platform=PACBIO --quality PHRED_33")
    LatfTest_OneInput( 7.2 0 "--platform=PACBIO --quality PHRED_33")
    LatfTest_OneInput( 7.3 0 "--platform=PACBIO --quality PHRED_33")
    #   secondary read numbers other than 3
    LatfTest_OneInputPhred33( 8.0 0 )
    #   reject inconsistent secondary read numbers
    LatfTest_OneInputPhred33( 8.1 3 )
    #   newest Illumina tag line variation ("_" instead of " /")
    LatfTest_OneInputPhred33( 9.0 0 )
    #   accepting ascii quality values up to 126
    LatfTest_OneInputPhred33( 10.0 0 )
    # 2-file fasta, tag line Run.Spot.Read
    LatfTest( 11.0 0 "input/11.0.1.fasta input/11.0.2.fasta --quality PHRED_33" )
    # 2-file fastq, tag line Run.Spot.Read
    LatfTest( 11.1 0 "input/11.1.1.fastq input/11.1.2.fastq --quality PHRED_33" )

    # Illumina, ignore barcodes
    LatfTest_OneInput( 12.0 0 "--quality PHRED_33 --ignore-illumina-tags" )
    # Illumina, use barcodes as spotgroups
    LatfTest_OneInputPhred33( 12.1 0 )
    # Illumina, use barcodes as spotgroups, alternative format
    LatfTest_OneInputPhred33( 12.2 0 )

    # Consume our own dog food (13.0.fastq created by fastq-dump )
    LatfTest_OneInputPhred33( 13.0 0 )
    # Consume our own dog food (13.1.fastq created by fastq-dump -F )
    LatfTest_OneInputPhred33( 13.1 0 )
    # SRA-2932: 30 is the preferred value for qualities when reading FASTA
    LatfTest( 14.0 0 "input/14.0.fasta --quality PHRED_33" )
    # VDB-3154: buffer insufficient
    LatfTestInt( 15.0 0 "-x READ_SEG" "./input/15.0.fastq --quality PHRED_33" )
     # VDB-3413: multi-line reads and qualities
    LatfTest( 16.0 0 "input/16.0.fasta --quality PHRED_33 --platform=PACBIO" )
    LatfTest( 16.1 0 "input/16.1.fastq --quality PHRED_33 --platform=PACBIO" )

    # VDB-4746: file padded with \0s
    LatfTest_OneInputPhred33( 17.0 3 )

    # VDB-4839: hangs on reaching max errors

    # VDB-5212: prints a "KQueuePop failed"
    LatfTest_OneInputPhred33( 18.0 3 )

endif()
