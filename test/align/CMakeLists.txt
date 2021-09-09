add_compile_definitions( __mod__="test/align" )

# add_executable( test-load-index indextest )
# MSVS_StaticRuntime( test-load-index )
# target_link_libraries( test-load-index ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
# add_test( NAME Test_Align_load_index COMMAND test-load-index WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

message( "TODO: make align test work inside sra-tools" )

if( NOT WIN32 )
    add_executable( test-samextract test-samextract )
    MSVS_StaticRuntime( test-samextract )
    include_directories( ${CMAKE_SOURCE_DIR}/libs/align )
    target_link_libraries( test-samextract sam-extract ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
    add_test( NAME Test_Align_samextract COMMAND test-samextract WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()

if ( NGS_LIBDIR )
    add_executable( test-estimator pileup_estimator )
    MSVS_StaticRuntime( test-estimator )
    target_link_directories( test-estimator PUBLIC ${NGS_LIBDIR} )
    target_link_libraries( test-estimator ncbi-ngs-c++ ncbi-ngs ngs-c++ ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
    add_test( NAME SlowTest_Align_estimator COMMAND test-estimator WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endif()
