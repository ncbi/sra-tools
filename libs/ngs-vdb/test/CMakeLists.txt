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

include_directories ( ${CMAKE_SOURCE_DIR} )
include_directories ("${VDB_INCDIR}/../libs/ngs")

set ( TEST_LIBS "ktst;ncbi-ngs;${COMMON_LIBS_READ};${COMMON_LINK_LIBRARIES}" )          

if( WIN32 )
    add_link_options( "/ENTRY:mainCRTStartup" )
endif()

#-------------------------------------------------------------------------------
# tests for the underlying C APIs

# ngs-makedb is a fixture that creates test databases
add_executable ( ngs-makedb ngstest_makedb.cpp )
target_link_libraries ( ngs-makedb "ktst;kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" )

set(DATADIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
file( MAKE_DIRECTORY ${DATADIR} )
add_test ( NAME Ngs_MakeDB COMMAND ngs-makedb WORKING_DIRECTORY ${DATADIR} )
set_property(TEST Ngs_MakeDB PROPERTY FIXTURES_SETUP MakeDB)

add_definitions(-D__mod__="NGS_TEST")
add_definitions(-D__file__="__FILE__")
add_definitions(-D__fext__="cpp")

#AddExecutableTest ( test-ngs ngstest.cpp "${TEST_LIBS}" "" )
add_executable ( test-ngs ngstest.cpp )
target_link_libraries ( test-ngs "${TEST_LIBS}")
add_test ( NAME Test_Ngs COMMAND test-ngs WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_sra ngstest_sra.cpp )
target_link_libraries ( test-ngs_sra "${TEST_LIBS}")
add_test ( NAME Test_Ngs_Sra COMMAND test-ngs_sra WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_sradb ngstest_sradb.cpp )
target_link_libraries ( test-ngs_sradb "${TEST_LIBS}")
add_test ( NAME Test_Ngs_SraDb COMMAND test-ngs_sradb WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_csra1 ngstest_csra1.cpp )
target_link_libraries ( test-ngs_csra1 "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_Csra1 COMMAND test-ngs_csra1 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties( SlowTest_Ngs_Csra1 PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

add_executable ( test-ngs_csra1_readcollection ngstest_csra1_readcollection.cpp )
target_link_libraries ( test-ngs_csra1_readcollection "${TEST_LIBS}")
add_test ( NAME Test_Ngs_Csra1_ReadCollection COMMAND test-ngs_csra1_readcollection WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_csra1_refwin ngstest_csra1_refwin.cpp )
target_link_libraries ( test-ngs_csra1_refwin "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_Csra1_RefWin COMMAND test-ngs_csra1_refwin WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties( SlowTest_Ngs_Csra1_RefWin PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

add_executable ( test-ngs_csra1_pileup ngstest_csra1_pileup.cpp )
target_link_libraries ( test-ngs_csra1_pileup "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_Csra1_Pileup COMMAND test-ngs_csra1_pileup WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_reference ngstest_reference.cpp )
target_link_libraries ( test-ngs_reference "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_Reference COMMAND test-ngs_reference WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable ( test-fragmentblob ngstest_fragmentblob.cpp )
target_link_libraries ( test-fragmentblob "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_FragmentBlob COMMAND test-fragmentblob WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# SlowTest_Ngs_FragmentBlob requires MakeDB to be run first
set_property(TEST SlowTest_Ngs_FragmentBlob PROPERTY FIXTURES_REQUIRED MakeDB)

add_executable ( test-referenceblob ngstest_referenceblob.cpp )
target_link_libraries ( test-referenceblob "${TEST_LIBS}")
add_test ( NAME Test_Ngs_ReferenceBlob COMMAND test-referenceblob WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-byteblob ngstest_byteblob.cpp )
target_link_libraries ( test-byteblob "${TEST_LIBS}")
add_test ( NAME Test_Ngs_ByteBlob COMMAND test-byteblob WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

#-------------------------------------------------------------------------------
# tests for the NGS C++ APIs

add_executable ( test-ngs_sra_c++ SRAtest.cpp )
target_link_libraries ( test-ngs_sra_c++ "${TEST_LIBS}")
add_test ( NAME Test_Ngs_CPP_Sra COMMAND test-ngs_sra_c++ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_sradb_c++ SRADBtest.cpp )
target_link_libraries ( test-ngs_sradb_c++ "${TEST_LIBS}")
add_test ( NAME Test_Ngs_CPP_SraDb COMMAND test-ngs_sradb_c++ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable ( test-ngs_csra1_c++ CSRA1test.cpp )
target_link_libraries ( test-ngs_csra1_c++ "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_CPP_CSRA1 COMMAND test-ngs_csra1_c++ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties( SlowTest_Ngs_CPP_CSRA1 PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

add_executable ( test-ngs_csra1pileup++ CSRA1PileupTest.cpp )
target_link_libraries ( test-ngs_csra1pileup++ "${TEST_LIBS}")
add_test ( NAME SlowTest_Ngs_CPP_CSRA1Pileup COMMAND test-ngs_csra1pileup++ WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties( SlowTest_Ngs_CPP_CSRA1Pileup PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

#-------------------------------------------------------------------------------
# tests for this library, ngs-vdb

add_executable ( test-ngs-vdb test-ngs-vdb.cpp )
add_dependencies ( test-ngs-vdb ngs-vdb )

target_link_libraries ( test-ngs-vdb
    tk-version
    ngs-vdb
    "${TEST_LIBS}"
)

add_test ( NAME SlowTest_NgsVdbWhiteBox COMMAND test-ngs-vdb )
