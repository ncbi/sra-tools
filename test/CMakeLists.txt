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

if ( NOT SINGLE_CONFIG )
	set( COMMON_LINK_LIBRARIES ${COMMON_LINK_LIBRARIES} ktst )
else()
	set( COMMON_LINK_LIBRARIES ${COMMON_LINK_LIBRARIES} ${NCBI_VDB_LIBDIR}/libktst.${STLX} )
endif()

add_subdirectory( external )
if( BUILD_TOOLS_LOADERS )
	add_subdirectory( loaders )
endif()
if( BUILD_TOOLS_INTERNAL )
	add_subdirectory( internal )
endif()
# if( BUILD_TOOLS_TEST_TOOLS )
	# add_subdirectory( test-tools )
# endif()
add_subdirectory( ngs )

add_subdirectory( sam-dump )

add_subdirectory(test_sanitizers)

add_subdirectory(align)
# # add_subdirectory(samline) # command line tool; needs arguments to become a test?
add_subdirectory(scripts)
# # add_subdirectory(tarballs) # called directly from TeamCity, do not invoke from CMake?
# # add_subdirectory(tui-example) # command line demos
# add_subdirectory(network-test) # broken now
# add_subdirectory(pileup-stats) # moved to ngs-tools, delete
# add_subdirectory(kget) VDB-4809 moved to ncbi-vdb, then retired (branch VDB-4809-preserving-vdb-get-aka-kget)
