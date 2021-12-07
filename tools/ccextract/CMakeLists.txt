add_compile_definitions( __mod__="tools/ccextract" )

# External
if( LibXml2_FOUND )
	add_executable( ccextract ccextract )
	target_link_libraries( ccextract
        ${COMMON_LINK_LIBRARIES}
        ${NCBI_VDB_ILIBDIR}/libkxfs.${STLX}
        ${NCBI_VDB_ILIBDIR}/libkxml.${STLX}
        ${COMMON_LIBS_READ}
        xml2
    )
	MakeLinksExe( ccextract false )
endif()
