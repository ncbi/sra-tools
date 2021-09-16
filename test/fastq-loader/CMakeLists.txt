add_executable( test-id2name test-id2name.cpp )
target_link_libraries( test-id2name fastqloader ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
add_test( NAME SlowTest_FastqLoader_id2name COMMAND test-id2name WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( wb-test-fastq wb-test-fastq.cpp )
target_link_libraries( wb-test-fastq fastqloader load ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
add_test( NAME Test_FastqLoader_WbFastq COMMAND wb-test-fastq WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( wb-test-fastq-parse wb-test-fastq-parse.cpp )
target_link_libraries( wb-test-fastq-parse fastqloader load ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
add_test( NAME Test_FastqLoader_WbFastqParse COMMAND wb-test-fastq-parse WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( test-fastq-loader test-fastq-loader.cpp )
target_link_libraries( test-fastq-loader fastqloader load ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
add_test( NAME Test_FastqLoader_WbFastqLoader COMMAND test-fastq-loader WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

if( NOT WIN32 )

    macro(LatfTest TestName Result TestParams)
        add_test( NAME Test_FastqLoader_${TestName}
            COMMAND "./runtestcase.sh" "${CMAKE_BINARY_DIR}/bin" "." "${TestName}" "${Result}" "${TestParams}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endmacro()

    macro(LatfTest_OneInput TestName Result TestParams)
        LatfTest( ${TestName} ${Result} "${CMAKE_CURRENT_SOURCE_DIR}/input/${TestName}.fastq ${TestParams}" )
    endmacro()

    macro(LatfTest_OneInputPhred33 TestName Result)
        LatfTest_OneInput( ${TestName} ${Result} "--quality PHRED_33" )
    endmacro()

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
    LatfTest_OneInputPhred33( 2.7 0 ) # spot assembly, read numbers not specified
#    LatfTest_OneInputPhred33( 2.8 0 ) # handling of N in reads, ALTREAD is a static column. TODO: update for the new schema; investigate intermittent failure (FIXED_SPOT_LEN == 0, not 3)
    LatfTest_OneInputPhred33( 2.8.1 0 ) # handling of N in reads, ALTREAD is a physical column
    LatfTest_OneInputPhred33( 2.9 3 ) # mismatching length of sequence and quality (qualities shorter)
    LatfTest_OneInputPhred33( 2.9.1 3 ) # mismatching length of sequence and quality (qualities longer)

#       Multiple files
    LatfTest( 3.1 0 "input/3.1a.fastq input/3.1b.fastq --quality PHRED_33") # reads+qualities together
#    LatfTest( 3.2 0 "input/3.2a.fastq input/3.2b.fastq --quality PHRED_33") # qualities separately from reads, reads file comes first
#    LatfTest( 3.3 0 "input/3.3a.fastq input/3.3b.fastq --quality PHRED_33" ) # qualities separately from reads, qualities file comes first (reject?)
#    LatfTest( 3.4 0 "-1 input/3.4a.fastq -2 input/3.4b.fastq --quality PHRED_33" ) # default read number TODO: uncomment when VDB-4530 merges

endif()