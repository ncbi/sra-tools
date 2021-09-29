include_directories( ${CMAKE_SOURCE_DIR}/ngs )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

link_libraries( ${COMMON_LINK_LIBRARIES} ncbi-ngs-c++ ncbi-vdb ngs-bind-c++ ngs-disp ngs )

add_executable( FragTest FragTest.cpp )
add_executable( AlignTest AlignTest.cpp )
add_executable( AlignSliceTest AlignSliceTest.cpp )
add_executable( PileupTest PileupTest.cpp )
add_executable( RefTest RefTest.cpp )
add_executable( DumpReferenceFASTA DumpReferenceFASTA.cpp )

if ( NOT WIN32 )
	add_test( NAME SlowTest_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_BINARY_DIR}/ngs/ngs-sdk/examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()