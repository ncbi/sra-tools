#if( WIN32 )
#	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
#endif()

include_directories( ${CMAKE_SOURCE_DIR}/ngs )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

if( WIN32 )
	#set( EXAMPLE_LIBS tk-version kapp ngs ngs-sdk ngs-c++ ncbi-ngs-c++ ncbi-vdb Ws2_32 Crypt32 )
	set( EXAMPLE_LIBS kapp tk-version ngs ncbi-ngs-c++ ncbi-vdb ngs-bind-c++ ngs-disp Ws2_32 Crypt32 )
else()
	set( EXAMPLE_LIBS kapp tk-version ncbi-ngs-c++ ncbi-vdb ngs-bind-c++ ngs-disp ngs )
endif()

#if( WIN32 )
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
#endif()

message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("CMAKE_TEST_OUTPUT_DIRECTORY: ${CMAKE_TEST_OUTPUT_DIRECTORY}")

# TODO: consider using link_libraries instead of target_link_libraries

add_executable( FragTest FragTest.cpp )
target_link_libraries( FragTest ${EXAMPLE_LIBS} )
MSVS_StaticRuntime( FragTest )

add_executable( AlignTest AlignTest.cpp )
target_link_libraries( AlignTest ${EXAMPLE_LIBS} ngs-c++ ngs-sdk )
MSVS_StaticRuntime( AlignTest )

add_executable( AlignSliceTest AlignSliceTest.cpp )
# target_link_directories( AlignSliceTest PUBLIC ${NCBI_VDB_LIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NCBI_VDB_ILIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NGS_LIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NGS_ILIBDIR} )
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


#add_test( NAME Test_AlignSliceTest COMMAND AlignSliceTest SRR1121656 1 1 9999 $(REDIRECT) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#add_test( NAME Test_AlignSliceTest COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_TEST_OUTPUT_DIRECTORY}/AlignSliceTest AlignSliceTest WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#add_test( NAME Test_AlignSliceTest COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh "./ngs/ngs-sdk/examples/AlignSliceTest_exe" "AlignSliceTest" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )


get_target_property( TEST_BINARY_DIRECTORY FragTest RUNTIME_OUTPUT_DIRECTORY)
message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# TODO: remove hardcoding for ngs/ngs/examples directory
add_test( NAME SlowTest_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_BINARY_DIR}/ngs/ngs-sdk/examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
