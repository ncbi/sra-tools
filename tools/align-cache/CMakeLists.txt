add_compile_definitions( __mod__="tools/align-cache" )

set( SRC
    align-cache
    helper
)

add_executable( align-cache ${SRC} )
target_link_libraries ( align-cache ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} load pthread dl m )
MakeLinksExe( align-cache true )