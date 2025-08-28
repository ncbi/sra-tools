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

AddExecutableTest ( Test_Ngs                        ngstest.cpp "${TEST_LIBS}" "" )
AddExecutableTest ( Test_Ngs_Sra                    ngstest_sra.cpp  "${TEST_LIBS}" "")
AddExecutableTest ( Test_Ngs_SraDb                  ngstest_sradb.cpp  "${TEST_LIBS}" "")

AddExecutableTest ( SlowTest_Ngs_Csra1_dflt ngstest_csra1.cpp "${TEST_LIBS}" "")
set_tests_properties( SlowTest_Ngs_Csra1_dflt
                      PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

AddExecutableTest ( Test_Ngs_Csra1_ReadCollection   ngstest_csra1_readcollection.cpp  "${TEST_LIBS}" "")

AddExecutableTest ( SlowTest_Ngs_Csra1_RefWin       ngstest_csra1_refwin.cpp  "${TEST_LIBS}" "")
set_tests_properties( SlowTest_Ngs_Csra1_RefWin
                      PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

AddExecutableTest ( SlowTest_Ngs_Csra1_Pileup       ngstest_csra1_pileup.cpp  "${TEST_LIBS}" "")
AddExecutableTest ( SlowTest_Ngs_Reference          ngstest_reference.cpp  "${TEST_LIBS}" "")

AddExecutableTest ( SlowTest_Ngs_FragmentBlob  ngstest_fragmentblob.cpp  "${TEST_LIBS}" "")
set_property(TEST SlowTest_Ngs_FragmentBlob PROPERTY FIXTURES_REQUIRED MakeDB)

AddExecutableTest ( Test_Ngs_ReferenceBlob  ngstest_referenceblob.cpp  "${TEST_LIBS}" "")
AddExecutableTest ( Test_Ngs_ByteBlob       ngstest_byteblob.cpp  "${TEST_LIBS}" "")

#-------------------------------------------------------------------------------
# tests for the NGS C++ APIs

AddExecutableTest ( Test_Ngs_CPP_Sra                SRAtest.cpp  "${TEST_LIBS}" "")
AddExecutableTest ( Test_Ngs_CPP_SraDb              SRADBtest.cpp  "${TEST_LIBS}" "")

AddExecutableTest ( SlowTest_Ngs_CPP_CSRA1_dflt CSRA1test.cpp "${TEST_LIBS}" "")
set_tests_properties( SlowTest_Ngs_CPP_CSRA1_dflt
                      PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

AddExecutableTest ( SlowTest_Ngs_CPP_CSRA1Pileup    CSRA1PileupTest.cpp  "${TEST_LIBS}" "")
set_tests_properties( SlowTest_Ngs_CPP_CSRA1Pileup PROPERTIES ENVIRONMENT "NCBI_VDB_QUALITY=R" )

#-------------------------------------------------------------------------------
# tests for this library, ngs-vdb

AddExecutableTest( SlowTest_NgsVdbWhiteBox test-ngs-vdb.cpp "ngs-vdb;${TEST_LIBS}" "" )
add_dependencies ( SlowTest_NgsVdbWhiteBox ngs-vdb )

