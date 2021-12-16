include_directories( ${CMAKE_SOURCE_DIR}/ngs )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

if (WIN32)
    set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup" )
endif()

link_libraries( ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ncbi-ngs-c++ ngs-bind-c++ ngs-disp ngs )

add_executable( FragTest FragTest.cpp )
add_executable( AlignTest AlignTest.cpp )
add_executable( AlignSliceTest AlignSliceTest.cpp )
add_executable( PileupTest PileupTest.cpp )
add_executable( RefTest RefTest.cpp )
add_executable( DumpReferenceFASTA DumpReferenceFASTA.cpp )

if ( NOT WIN32 )
	add_test( NAME SlowTest_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${BINDIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()