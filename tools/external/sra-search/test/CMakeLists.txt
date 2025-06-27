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

# this is where test executables will be created
# to access tools themselves, use ${BINDIR}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TESTBINDIR} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TESTBINDIR_DEBUG} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TESTBINDIR_RELEASE} )

include_directories ("..")

# test-sra-search
if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup ")
endif ()

include_directories ( ${CMAKE_SOURCE_DIR}/ngs )
link_libraries (
    ngs-vdb
    ncbi-ngs
    ${CMAKE_STATIC_LIBRARY_PREFIX}ktst${CMAKE_STATIC_LIBRARY_SUFFIX}
	${COMMON_LIBS_READ}
    ${COMMON_LINK_LIBRARIES}
)

AddExecutableTest ( Test_SraSearch
    "test-sra-search.cpp;../vdb-search.cpp;../searchblock.cpp;../blobmatchiterator.cpp;../fragmentmatchiterator.cpp;../referencematchiterator.cpp"
    "ksrch" "" "--no-asan"
)

AddExecutableTest ( Test_SraSearchSlow
    "test-sra-search-slow.cpp;../vdb-search.cpp;../searchblock.cpp;../blobmatchiterator.cpp;../fragmentmatchiterator.cpp;../referencematchiterator.cpp"
    "ksrch" ""
)

AddExecutableTest ( Test_SraSearch_SearchBlock
    "test-searchblock.cpp;../searchblock.cpp"
    "${COMMON_LIBS_READ};ksrch;ncbi-vdb" ""
)

# white box tests
add_test ( NAME Test_SraSearchBlockWhiteBox COMMAND Test_SraSearch_SearchBlock )
add_test ( NAME Test_SraSearchWhiteBox COMMAND Test_SraSearch )
add_test ( NAME SlowTest_SraSearchWhiteBox COMMAND Test_SraSearchSlow )
set_tests_properties(SlowTest_SraSearchWhiteBox PROPERTIES TIMEOUT 5000)

# command line tests
if ( NOT WIN32)
    ToolsRequired(sra-search)
    set ( SRA_SEARCH  ${DIRTOTEST}/sra-search${EXE} )

    # NB. Slow (run time > 1m) tests should names that start with "SlowTest_"; run without: "ctest -E SlowTest_", run only: "ctest -R SlowTest_"

	# help
	add_test ( NAME Test_SraSearch-1.0-h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 1.0-h --args "-h" )
	add_test ( NAME Test_SraSearch-1.1-help WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
			   	${SRA_SEARCH} 1.1-help --args "--help" )
	# version
    add_test ( NAME Test_SraSearch-1.2-V WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./execandgrep.sh "${SRA_SEARCH} -V" ${VERSION} )
    add_test ( NAME SraSearch-1.3-version WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./execandgrep.sh "${SRA_SEARCH} --version" ${VERSION} )

	# bad arguments
	add_test ( NAME Test_SraSearch-2.0-no-args WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 2.0-no-args --rc 1 )
	add_test ( NAME Test_SraSearch-2.1-no-run WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 2.1-no-run --rc 1 --args AGCT )
	add_test ( NAME Test_SraSearch-2.2-missing-algorithm WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 2.2-missing-algorithm --rc 1 --args "AGCT SRR000001 -a" )
	add_test ( NAME Test_SraSearch-2.3-bad-algorithm WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 2.3-bad-algorithm --rc 1 --args "--algorithm bad AGCT SRR000001" )

	# single run
	add_test ( NAME SlowTest_SraSearch-3.0-single-run-default WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.0-single-run-default            --args "AGCTAGCTAGCT -T 1 SRR000001" )
	add_test ( NAME Test_SraSearch-3.05-single-run-default-WGS WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.05-single-run-default-WGS --sort --args "AGCTAGCTA ALAI01" )
	add_test ( NAME SlowTest_SraSearch-3.1-single-run-fgrep-standard WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.1-single-run-fgrep-standard     --args "AGCTAGCTAGCT -T 1 -a FgrepStandard SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.2-single-run-fgrep-boyer-moore WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.2-single-run-fgrep-boyer-moore  --args "AGCTAGCTAGCT -T 1 --algorithm FgrepBoyerMoore    SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.3-single-run-fgrep-aho WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.3-single-run-fgrep-aho          --args "AGCTAGCTAGCT -T 1 --algorithm FgrepAho           SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.4-single-run-agrep-dp WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.4-single-run-agrep-dp           --args "AGCTAGCTAGCT -T 1 --algorithm AgrepDP            SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.5-single-run-agrep-wumanber WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.5-single-run-agrep-wumanber     --args "AGCTAGCTAGCT -T 1 --algorithm AgrepWuManber      SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.6-single-run-agrep-myers WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.6-single-run-agrep-myers        --args "AGCTAGCTAGCT -T 1 --algorithm AgrepMyers         SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.7-single-run-agrep-myers-unltd WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.7-single-run-agrep-myers-unltd  --args "AGCTAGCTAGCT -T 1 --algorithm AgrepMyersUnltd    SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.8-single-run-nucstrstr WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.8-single-run-nucstrstr          --args "AGCTAGCTAGCT -T 1 --algorithm NucStrstr          SRR000001" )
	add_test ( NAME SlowTest_SraSearch-3.9-single-run-smith-waterman WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.9-single-run-smith-waterman     --args "AGCTAGCTAGCT -T 1 --algorithm SmithWaterman -m 4 SRR000001" )

	# single run, fasta format
	add_test ( NAME Test_SraSearch-3.10-single-run-fasta WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.10-single-run-fasta --args "AGCTAGCTA ALAI01 --fasta --max 2" )
	add_test ( NAME Test_SraSearch-3.11-single-run-fasta-custom-line-size WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 3.11-single-run-fasta-custom-line-size --args "AGCTAGCTA ALAI01 --fasta 80 --max 2" )

	# multiple runs, single threaded
	add_test ( NAME Test_SraSearch-4.0-multiple-runs WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 4.0-multiple-runs --args "TACTGGGCGTAAAGCGTGCGCAGGCGGT -T 1 --algorithm NucStrstr SRR600096 SRR600097" )

	# query expressions
	add_test ( NAME SlowTest_SraSearch-5.0-run-nucstrstr-with-expression WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 5.0-run-nucstrstr-with-expression --sort --args "AAAAAAACCCCCCCAAAAAAACCCCCCC\\|\\|AGCTAGCTAGCT -T 1 --algorithm NucStrstr --expression  SRR000001" )

	# query expressions, bad arguments
	add_test ( NAME Test_SraSearch-5.1-run-not-nucstrstr-with-expression WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 5.1-run-not-nucstrstr-with-expression --args "AAAAAAACCCCCCCAAAAAAACCCCCCC\\|\\|AGCTAGCTAGCT --algorithm FgrepStandard --expression  SRR000001" --rc 1 )

	# imperfect match, bad arguments
	add_test ( NAME Test_SraSearch-6.0.1-imperfect-match-no-arg WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.0.1-imperfect-match-no-arg --args "AGCTAGCTAGCT SRR000001 -S" --rc 1 )
	add_test ( NAME Test_SraSearch-6.0.2-imperfect-match-bad-arg WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.0.2-imperfect-match-bad-arg --args "AGCTAGCTAGCT SRR000001 -S qq" --rc 1  )
	add_test ( NAME Test_SraSearch-6.0.3-imperfect-match-default WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
	         	${SRA_SEARCH} 6.0.3-imperfect-match-default --args "AGCTAGCTAGCT -S 90 SRR000001" --rc 1 )
	add_test ( NAME Test_SraSearch-6.1-imperfect-match-fgrep-standard  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
		    	${SRA_SEARCH} 6.1-imperfect-match-fgrep-standard --args "AGCTAGCTAGCT --score 90 -a FgrepStandard SRR000001" --rc 1  )
	add_test ( NAME Test_SraSearch-6.2-imperfect-match-fgrep-boyer-moore  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.2-imperfect-match-fgrep-boyer-moore --args "AGCTAGCTAGCT --score 90 --algorithm FgrepBoyerMoore SRR000001" --rc 1 )
	add_test ( NAME Test_SraSearch-6.3-imperfect-match-fgrep-aho  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
	         	${SRA_SEARCH} 6.3-imperfect-match-fgrep-aho --args "AGCTAGCTAGCT --score 90 --algorithm FgrepAho SRR000001" --rc 1 )
	# imperfect match
	add_test ( NAME SlowTest_SraSearch-6.4-imperfect-match-agrep-dp WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.4-imperfect-match-agrep-dp           --args "AGCTAGCTAGCT -T 1 --score 90 --algorithm AgrepDP            SRR000001" )
	add_test ( NAME SlowTest_SraSearch-6.5-imperfect-match-agrep-wumanber WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.5-imperfect-match-agrep-wumanber     --args "AGCTAGCTAGCT -T 1 --score 90 --algorithm AgrepWuManber      SRR000001" )
	add_test ( NAME SlowTest_SraSearch-6.6-imperfect-match-agrep-myers WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.6-imperfect-match-agrep-myers        --args "AGCTAGCTAGCT -T 1 --score 90 --algorithm AgrepMyers         SRR000001" )
	add_test ( NAME SlowTest_SraSearch-6.7-imperfect-match-agrep-myers-unltd WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.7-imperfect-match-agrep-myers-unltd  --args "AGCTAGCTAGCT -T 1 --score 90 --algorithm AgrepMyersUnltd    SRR000001" )
	add_test ( NAME Test_SraSearch-6.8-imperfect-match-nucstrstr  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
	         	${SRA_SEARCH} 6.8-imperfect-match-nucstrstr --args "AGCTAGCTAGCT --score 90 --algorithm NucStrstr SRR000001" --rc 1 )
	add_test ( NAME SlowTest_SraSearch-6.9-imperfect-match-smith-waterman WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 6.9-imperfect-match-smith-waterman     --args "AGCTAGCTAGCT -T 1 --score 90 --algorithm SmithWaterman -m 100      SRR000001" )

	# multi-threading, bad arguments
	add_test ( NAME Test_SraSearch-7.0.1-threads-no-arg WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
			 	${SRA_SEARCH} 7.0.1-threads-no-arg --rc 1 --args "AGCTAGCTAGCT SRR600094 SRR600095 -T" )
	add_test ( NAME Test_SraSearch-7.0.2-threads-bad-arg WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
			 	${SRA_SEARCH} 7.0.2-threads-bad-arg --rc 1 --args "AGCTAGCTAGCT SRR600094 SRR600095 --threads -1" )
	# multi-threading
	# multiple runs, thread per accession
	add_test ( NAME SlowTest_SraSearch-7.1-threads-unsorted WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 7.1-threads-unsorted --sort --args "ACGTAGGGTCC --threadperacc --threads 4 SRR600094 SRR600095 SRR600096 SRR600099" )
	# single run, thread per blob
	add_test ( NAME SlowTest_SraSearch-7.2-threads-single WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 7.2-threads-single --sort --args "ACGTAGGGTCC --threads 2 SRR600094" )
	# multiple runs, thread per blob
	add_test ( NAME SlowTest_SraSearch-7.3-threads-multiple WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 7.3-threads-multiple --sort --args "ACGTAGGGTCC --threads 2 SRR600094 SRR600095 SRR600096 SRR600099" )
	# sorting the output
	add_test ( NAME SlowTest_SraSearch-7.4-threads-sort WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 7.4-threads-sort --args "ACGTAGGGTCC --threads 4 SRR600096 SRR600095 --sort" )

	# nothing found
 	add_test ( NAME SlowTest_SraSearch-8.0-nothing-found WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
 				${SRA_SEARCH} 8.0-nothing-found --rc 3 --args "TTTTTTTTAAAAAAACCCC -T 1 SRR000001" )

	# reference mode
	add_test ( NAME SlowTest_SraSearch-9.0-reference WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 9.0-reference --sort --args "ACGTAGGGTCC SRR600094 --reference -T 6" )

	# reference mode, multiple references, one accession
	add_test ( NAME SlowTest_SraSearch-9.1-reference-multiple-refs WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 9.1-reference --sort --args "ACGTAGGGTCC SRR600094 --reference NC_000007.13,doesnotexist,NC_000001.10 -T 6" )

	# reference mode, multiple references, accessions
	add_test ( NAME SlowTest_SraSearch-9.2-reference-multiple-refs-multiple-runs WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 9.2-reference --sort --args "ACGTAGGGTCC SRR600095 SRR600094 --reference NC_000007.13,NC_000001.10 -T 6" )

	# reference mode, expressions not allowed
	add_test ( NAME Test_SraSearch-9.3-reference-expression WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 9.3-reference-expression --args "ACGTAGGGTCC SRR600094 --reference --expression" --rc 1)

    # unaligned reads only
	add_test ( NAME SlowTest_SraSearch-10.0-unaligned WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND sh runtestcase.sh
				${SRA_SEARCH} 10.0-unaligned --args "AGCTAGCTAGCT SRR600099 --unaligned" )

endif ()
