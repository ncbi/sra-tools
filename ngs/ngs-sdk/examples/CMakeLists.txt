include_directories( ${CMAKE_SOURCE_DIR}/ngs/ngs-sdk )

message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("CMAKE_TEST_OUTPUT_DIRECTORY: ${CMAKE_TEST_OUTPUT_DIRECTORY}")

# TODO: properly initialize those directories somewhere on the top level
set( NCBI_VDB_LIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/lib" )
set( NCBI_VDB_ILIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/ilib" )
set( NGS_LIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/lib" )
set( NGS_ILIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/ilib" )
link_directories( ${NCBI_VDB_LIBDIR} )
link_directories( ${NCBI_VDB_ILIBDIR} )
link_directories( ${NGS_LIBDIR} )
link_directories( ${NGS_ILIBDIR} )

# TODO: consider using link_libraries instead of target_link_libraries

add_executable( FragTest FragTest.cpp )
target_link_libraries( FragTest kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )

add_executable( AlignTest AlignTest.cpp )
target_link_libraries( AlignTest kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )

add_executable( AlignSliceTest AlignSliceTest.cpp )
# target_link_directories( AlignSliceTest PUBLIC ${NCBI_VDB_LIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NCBI_VDB_ILIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NGS_LIBDIR} )
# target_link_directories( AlignSliceTest PUBLIC ${NGS_ILIBDIR} )
target_link_libraries( AlignSliceTest kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )

add_executable( PileupTest PileupTest.cpp )
target_link_libraries( PileupTest kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )

add_executable( RefTest RefTest.cpp )
target_link_libraries( RefTest kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )

add_executable( DumpReferenceFASTA DumpReferenceFASTA.cpp )
target_link_libraries( DumpReferenceFASTA kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )


#add_test( NAME Test_AlignSliceTest COMMAND AlignSliceTest SRR1121656 1 1 9999 $(REDIRECT) WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#add_test( NAME Test_AlignSliceTest COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_TEST_OUTPUT_DIRECTORY}/AlignSliceTest AlignSliceTest WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
#add_test( NAME Test_AlignSliceTest COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh "./ngs/ngs-sdk/examples/AlignSliceTest_exe" "AlignSliceTest" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )


get_target_property( TEST_BINARY_DIRECTORY FragTest RUNTIME_OUTPUT_DIRECTORY)
message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# TODO: remove hardcoding for ngs/ngs/examples directory
add_test( NAME Test_NGS_Run_and_diff COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_and_diff.sh ${CMAKE_BINARY_DIR}/ngs/ngs-sdk/examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
