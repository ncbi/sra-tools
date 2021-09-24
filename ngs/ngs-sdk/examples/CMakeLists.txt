include_directories( ${CMAKE_SOURCE_DIR}/ngs )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

if( WIN32 )
	set( EXAMPLE_LIBS kapp tk-version ngs ncbi-ngs-c++ ncbi-vdb ngs-bind-c++ ngs-disp Ws2_32 Crypt32 )
else()
	set( EXAMPLE_LIBS kapp tk-version ncbi-ngs-c++ ncbi-vdb ngs-bind-c++ ngs-disp ngs )
endif()

add_executable( FragTest FragTest.cpp )
target_link_libraries( FragTest ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( FragTest )

add_executable( AlignTest AlignTest.cpp )
target_link_libraries( AlignTest ${EXAMPLE_LIBS} ngs-c++ ngs-sdk )
MSVS_StaticRuntime( AlignTest )

add_executable( AlignSliceTest AlignSliceTest.cpp )
target_link_libraries( AlignSliceTest ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( AlignSliceTest )

add_executable( PileupTest PileupTest.cpp )
target_link_libraries( PileupTest ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( PileupTest )

add_executable( RefTest RefTest.cpp )
target_link_libraries( RefTest ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( RefTest )

add_executable( DumpReferenceFASTA DumpReferenceFASTA.cpp )
#add_dependencies( DumpReferenceFASTA ngs-bind-c++ )
target_link_libraries( DumpReferenceFASTA ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( DumpReferenceFASTA )

#get_target_property( TEST_BINARY_DIRECTORY FragTest RUNTIME_OUTPUT_DIRECTORY)
#message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if ( NOT WIN32 )
	# TODO: remove hardcoding for ngs/ngs/examples directory
	add_test( NAME SlowTest_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_BINARY_DIR}/ngs/ngs-sdk/examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()