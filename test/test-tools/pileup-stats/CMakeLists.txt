add_test ( NAME PileupStats-Version
           COMMAND sh -c "${BINDIR}/pileup-stats -V | grep -q `head -n 1 ${CMAKE_SOURCE_DIR}/shared/toolkit.vers`"
    )
add_test ( NAME PileupStats-Help COMMAND sh -c "${BINDIR}/pileup-stats -h | grep -q Usage" )

add_test ( NAME PileupStats-StdOptions COMMAND sh -c "${BINDIR}/pileup-stats -+VDB" )

macro( ErrorTest name command)
    add_test ( NAME ${name} COMMAND sh -c "${command}" )
    set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE)
endmacro()

# no arguments
ErrorTest ( PileupStats-NoArgs ${BINDIR}/pileup-stats )

# bad arguments and missing run names
ErrorTest ( PileupStats-IgnoreArgs-L        "${BINDIR}/pileup-stats -L 1" )
ErrorTest ( PileupStats-IgnoreArgs-z        "${BINDIR}/pileup-stats -z ignore.txt" )
ErrorTest ( PileupStats-IgnoreArgs-loglevel "${BINDIR}/pileup-stats --log-level 1" )
ErrorTest ( PileupStats-IgnoreArgs-xmllog   "${BINDIR}/pileup-stats --xml-log ignore.txt" )
ErrorTest ( PileupStats-IgnoreArgs-xmllogfd "${BINDIR}/pileup-stats --xml-log-fd ignore.txt" )
