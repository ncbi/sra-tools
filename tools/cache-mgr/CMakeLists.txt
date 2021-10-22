add_compile_definitions( __mod__="tools/cache-mgr" )

# External
add_executable( cache-mgr cache-mgr )
target_link_libraries( cache-mgr ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( cache-mgr true )
