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

add_compile_definitions( __mod__="test/sra-add-fp" )

ToolsRequired( sra-add-fp kar fastq-dump kdbmeta )

# help
add_test( NAME Test_Sra_Add_Fp_help
	      COMMAND ${DIRTOTEST}/sra-add-fp${EXE} -h
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# bad parameters
add_test( NAME Test_Sra_Add_Fp_missing_run
	      COMMAND ${DIRTOTEST}/sra-add-fp${EXE} input/missing
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(Test_Sra_Add_Fp_missing_run PROPERTIES WILL_FAIL TRUE)

add_test( NAME Test_Sra_Add_Fp_invalid_run
	      COMMAND ${DIRTOTEST}/sra-add-fp${EXE} CMakeLists.txt
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(Test_Sra_Add_Fp_invalid_run PROPERTIES WILL_FAIL TRUE)

add_test( NAME Test_Sra_Add_Fp_readonly_run
	      COMMAND ${DIRTOTEST}/sra-add-fp${EXE} input/Test_DB.sra
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set_tests_properties(Test_Sra_Add_Fp_readonly_run PROPERTIES WILL_FAIL TRUE)

# business logic

# database
add_test( NAME Test_Sra_Add_Fp_database_unaligned
	      COMMAND ./runtestcase.sh ${DIRTOTEST} . Test_DB
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# table; filtering out tech reads
# NB.Test_Table is a copy of SRR139146
add_test( NAME Test_Sra_Add_Fp_table_with_tech
	      COMMAND ./runtestcase.sh ${DIRTOTEST} . Test_Table
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#fp already present
add_test( NAME Test_Sra_Add_Fp_database_fp_already_present
	      COMMAND ./runtestcase.sh ${DIRTOTEST} . Test_DB_with_fp
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#TODO:
# aligned database run
#