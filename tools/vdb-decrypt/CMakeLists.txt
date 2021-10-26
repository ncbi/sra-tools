add_compile_definitions( __mod__="tools/vdb-decrypt" )

# External
add_executable( vdb-decrypt
	vdb-decrypt
	shared
)
target_link_libraries( vdb-decrypt ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-decrypt true )

add_executable( vdb-encrypt
	vdb-encrypt
	shared
)
target_link_libraries( vdb-encrypt ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-encrypt true )
