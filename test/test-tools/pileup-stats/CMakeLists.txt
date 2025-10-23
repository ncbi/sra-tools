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

ToolsRequired(pileup-stats)

add_test ( Test_pileup-stats-Version
    sh -c "${BINDIR}/pileup-stats -V |
        grep -q `head -n 1 ${CMAKE_SOURCE_DIR}/shared/toolkit.vers`"
    )
add_test ( Test_pileup-stats-Help
    sh -c "${BINDIR}/pileup-stats -h | grep -q Usage"
    )

# no arguments
add_test ( Test_pileup-stats-NoArgs              sh -c "${BINDIR}/pileup-stats; if [ $? == 0 ] ; then exit 1; fi;" )

# bad arguments and missing run names
add_test ( Test_pileup-stats-IgnoreArgs-L        sh -c "${BINDIR}/pileup-stats -L 1; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( Test_pileup-stats-IgnoreArgs-z        sh -c "${BINDIR}/pileup-stats -z ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( Test_pileup-stats-IgnoreArgs-loglevel sh -c "${BINDIR}/pileup-stats --log-level 1; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( Test_pileup-stats-IgnoreArgs-xmllog   sh -c "${BINDIR}/pileup-stats --xml-log ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( Test_pileup-stats-IgnoreArgs-xmllogfd sh -c "${BINDIR}/pileup-stats --xml-log-fd ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
