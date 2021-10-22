add_compile_definitions( __mod__="tools/kget" )

add_executable( vdb-get kget )
target_link_libraries( vdb-get ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
