add_compile_definitions( __mod__="tools/rd-filter-redact" )

# Internal
add_executable( test-read-write-cursor test-read-write-cursor )
target_link_libraries ( test-read-write-cursor ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( test-read-write-cursor true )

# External
add_executable( read-filter-redact read-filter-redact )
target_link_libraries ( read-filter-redact ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( read-filter-redact true )
