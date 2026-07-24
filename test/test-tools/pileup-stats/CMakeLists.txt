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

if( NOT WIN32 )

    ToolsRequired( pileup-stats )

    set( PILEUP_STATS ${DIRTOTEST}/pileup-stats )

	add_test ( NAME Test_PileupStats-1.0-h COMMAND ${EXECANDGREP} "${PILEUP_STATS} -h" "Output brief explanation" )
	add_test ( NAME Test_PileupStats-1.1-help COMMAND ${EXECANDGREP} "${PILEUP_STATS} --help" "Output brief explanation" )

    # version
    add_test ( NAME Test_PileupStats-1.2-V COMMAND ${EXECANDGREP} "${PILEUP_STATS} -V" ${VERSION} )
    add_test ( NAME Test_PileupStats-1.3-version COMMAND ${EXECANDGREP} "${PILEUP_STATS} --version" ${VERSION} )

	# other standard options
    add_test ( NAME Test_PileupStats-1.4-Standard WORKING_DIRECTORY COMMAND ${EXECANDGREP} "${PILEUP_STATS} SRR000123 -L debug" "requesting guard size" )

    add_test( NAME SlowTest_pileup-stats-optional-column-ref-offset-type
            COMMAND bash -c "pileup-stats --align-category primary ERR085308 >/dev/null"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()
