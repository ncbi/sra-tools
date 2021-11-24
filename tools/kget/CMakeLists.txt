add_compile_definitions( __mod__="tools/kget" )

add_executable( vdb-get kget )
target_link_libraries( vdb-get ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-get false )
