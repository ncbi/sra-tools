add_compile_definitions( __mod__="tools/align-info" )

set( SRC
    align-info
)

add_executable( align-info ${SRC} )
target_link_libraries ( align-info ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( align-info true )