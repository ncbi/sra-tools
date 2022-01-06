add_compile_definitions( __mod__="test/align" )

include_directories( ${VDB_INTERFACES_DIR}/ext/ ) # zlib.h

# AddExecutableTest( Test_Align_load_index "indextest" "${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

if( NOT WIN32 )
    AddExecutableTest( Test_Align_samextract "test-samextract" "sam-extract;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )
endif()

add_executable( test-estimator pileup_estimator )
include_directories( ${CMAKE_SOURCE_DIR} )
target_link_libraries( test-estimator ncbi-ngs-c++ ngs-c++ ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
add_test( NAME SlowTest_Align_estimator COMMAND test-estimator WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
