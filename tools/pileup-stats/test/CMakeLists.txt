add_test ( PileupStats-Version
    sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats -V |
        grep --quiet `head -n 1 ${CMAKE_SOURCE_DIR}/libs/tk-version/toolkit.vers`"
    )
add_test ( PileupStats-Help
    sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats -h | grep --quiet Usage"
    )

# no arguments
add_test ( PileupStats-NoArgs              sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats; if [ $? == 0 ] ; then exit 1; fi;" )

# bad arguments and missing run names
add_test ( PileupStats-IgnoreArgs-L        sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats -L 1; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( PileupStats-IgnoreArgs-z        sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats -z ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( PileupStats-IgnoreArgs-loglevel sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats --log-level 1; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( PileupStats-IgnoreArgs-xmllog   sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats --xml-log ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
add_test ( PileupStats-IgnoreArgs-xmllogfd sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pileup-stats --xml-log-fd ignore.txt; if [ $? == 0 ] ; then exit 1; fi;" )
