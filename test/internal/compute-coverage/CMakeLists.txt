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

# command line tests
if ( NOT WIN32)

	ToolsRequired( compute-coverage )

    set( COMPUTE_COVERAGE ${DIRTOTEST}/compute-coverage )

	# help
	add_test ( NAME Test_ComputeCoverage-1.0-h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	            COMMAND runtestcase.sh ${COMPUTE_COVERAGE} 1.0-h --args "-h" )
	add_test ( NAME Test_ComputeCoverage-1.1-help WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	            COMMAND runtestcase.sh ${COMPUTE_COVERAGE} 1.1-help --args "--help" )
	# version
    add_test ( NAME Test_ComputeCoverage-1.2-V WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ../../execandgrep.sh "${COMPUTE_COVERAGE} -V" ${VERSION} )
    add_test ( NAME Test_ComputeCoverage-1.3-version WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ../../execandgrep.sh "${COMPUTE_COVERAGE} --version" ${VERSION} )

	# -+
    add_test ( NAME Test_ComputeCoverage-1.4-Debug WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ../../execandgrep.sh "${COMPUTE_COVERAGE} SRR496123 -+VDB" "VDatabaseOpenRead" )

endif ()
