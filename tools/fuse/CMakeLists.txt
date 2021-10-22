add_compile_definitions( __mod__="tools/fuse" )

set ( UNIX_SRC unix/remote-fuser-sys.c unix/sra-fuser-sys.c )
set ( LINUX_SRC ${UNIX_SRC} )
set ( MAC_SRC ${UNIX_SRC} )
set ( WIN_SRC )

if ( "windows" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${WIN_SRC} )
    include_directories( win )
elseif( "linux" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${LINUX_SRC} )
    include_directories( unix )
elseif( "mac" STREQUAL ${OS} )
    set ( OS_SPECIFIC_SRC ${MAC_SRC} )
    include_directories( unix )
endif()

# External
if( LibXml2_FOUND  )
	add_executable( sra-fuser
		zlib-simple
		log
		node
		tar-list
		file
		tar-file
		tar-node
		directory
		formats
		accessor
		kfile-accessor
		xml
		text-file
		sra-list
		sra-directory
		sra-node
		sra-fastq
		sra-sff
		sra-fuser
		${OS_SPECIFIC_SRC}
	)
	target_link_libraries( sra-fuser ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m xml2 )

	add_executable( remote-fuser
		log
		node
		accessor
		kfile-accessor
		remote-xml
		remote-cache
		remote-link
		remote-file
		remote-directory
		remote-fuser
		${OS_SPECIFIC_SRC}
	)
	target_link_libraries( remote-fuser ${COMMON_LINK_LIBRARIES} ncbi-wvdb.${STLX} pthread dl m srareader xml2 )
endif()