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

add_subdirectory(align)
add_subdirectory(align-cache)
add_subdirectory(align-info)
add_subdirectory(copycat) # fails
add_subdirectory(driver-tool)
add_subdirectory(fasterq-dump)
add_subdirectory(fastq-loader)
add_subdirectory(kar)
add_subdirectory(kdbmeta)
add_subdirectory(kget)
add_subdirectory(make-read-filter)
add_subdirectory(ngs-java)
add_subdirectory(ngs-pileup)
add_subdirectory(ngs-python)
add_subdirectory(prefetch) # TODO ascp ?
add_subdirectory(read-filter-redact)
# # add_subdirectory(samline) # command line tool; needs arguments to become a test?
add_subdirectory(scripts)
add_subdirectory(sharq)
add_subdirectory(sra-pileup)
add_subdirectory(sra-sort) # TODO: it's not clear if the test itself was running, now it's not.
add_subdirectory(sra-stat)
add_subdirectory(srapath) # TODO: some variables have no effect on the test
# # add_subdirectory(tarballs) # called directly from TeamCity, do not invoke from CMake?
# # add_subdirectory(tui-example) # command line demos
add_subdirectory(vdb-config)
add_subdirectory(vdb-copy)
add_subdirectory(vdb-diff) # was not included in runtests
add_subdirectory(vdb-dump)
add_subdirectory(vdb-validate)
add_subdirectory(test_sanitizers)

# add_subdirectory(network-test) # broken now
# add_subdirectory(pileup-stats) # moved to ngs-tools, delete
