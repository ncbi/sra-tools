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

if ( CMAKE_BUILD_TYPE STREQUAL "Debug" )
if ( WIN32 )
    add_executable( Drivertool_Win32_Args_Parsing_Test win32-cmdline-test.cpp )
    target_include_directories( Drivertool_Win32_Args_Parsing_Test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/driver-tool
    )
    add_test( NAME Test_Drivertool_Win32_Args_Parsing COMMAND Drivertool_Win32_Args_Parsing_Test )
else ()
	# GOOD
	add_test( NAME Test_Drivertool_Fastq_dump
		COMMAND good_test.sh fastq-dump "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasterq_dump
		COMMAND good_test.sh fasterq-dump "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Sam_dump
		COMMAND good_test.sh sam-dump "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Sra_pileup
		COMMAND good_test.sh sra-pileup "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Vdb_dump
		COMMAND good_test.sh vdb-dump "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Prefetch
		COMMAND good_test.sh prefetch "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Srapath
		COMMAND good_test.sh srapath "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# CONTAINER
	add_test( NAME Test_Drivertool_Container_SRP000001
		COMMAND container_test.sh SRP000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Container_SRX000001
		COMMAND container_test.sh SRX000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Container_SRS000001
		COMMAND container_test.sh SRS000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Container_SRA000001
		COMMAND container_test.sh SRA000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Container_ERP000001
		COMMAND container_test.sh ERP000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Container_DRX000001
		COMMAND container_test.sh DRX000001 "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# NO_SDL
	add_test( NAME Test_Drivertool_NO_SDL
		COMMAND no_sdl.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	# bogus
	add_test( NAME Test_Drivertool_Bogus
		COMMAND bogus.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Testing
		COMMAND testing.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Vdbcache
		COMMAND vdbcache.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Split_3
		COMMAND split_3.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Split_e
		COMMAND split_e.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasta_missing_param
		COMMAND fasta_missing_param.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasta_default
		COMMAND fasta_default.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasta_0
		COMMAND fasta_0.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasta_number
		COMMAND fasta_number.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Fasta_neg_number
		COMMAND fasta_neg_number.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Help_srapath
		COMMAND help_srapath.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Help_prefetch
		COMMAND help_prefetch.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Help_fasterq-dump
		COMMAND help_fasterq-dump.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_Help_fastq-dump
		COMMAND help_fastq-dump.sh "${DIRTOTEST}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_fasterq-dump_no-fasta
		COMMAND fasta_er.sh "${DIRTOTEST}" "no_fasta_er" ""
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_fasterq-dump_fasta
		COMMAND fasta_er.sh "${DIRTOTEST}" "fasta_er" "--fasta"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_fasterq-dump_fasta-unsorted
		COMMAND fasta_er.sh "${DIRTOTEST}" "fasta_er_unsorted" "--fasta-unsorted"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

	add_test( NAME Test_Drivertool_2_accessions
		COMMAND two_accessions.sh "${DIRTOTEST}" "two_accessions"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()
endif()
