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
	add_test ( NAME Test_ComputeCoverage-1.0-h COMMAND ${EXECANDGREP} "${COMPUTE_COVERAGE} -h" "Output brief explanation" )
	add_test ( NAME Test_ComputeCoverage-1.1-help Test_PileupStats "${COMPUTE_COVERAGE} --help" "Output brief explanation" )

    # version
    add_test ( NAME Test_ComputeCoverage-1.2-V COMMAND ${EXECANDGREP} "${COMPUTE_COVERAGE} -V" ${VERSION} )
    add_test ( NAME Test_ComputeCoverage-1.3-version COMMAND ${EXECANDGREP} "${COMPUTE_COVERAGE} --version" ${VERSION} )

	# other standard options
    add_test ( NAME Test_ComputeCoverage-1.4-Standard WORKING_DIRECTORY COMMAND ${EXECANDGREP} "${COMPUTE_COVERAGE} SRR496123 -L debug" "requesting guard size" )

endif ()
