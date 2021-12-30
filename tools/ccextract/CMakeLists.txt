add_compile_definitions( __mod__="tools/ccextract" )

# External
if( LibXml2_FOUND )
	add_executable( ccextract ccextract )
	if( SINGLE_CONFIG )
		target_link_libraries( ccextract
		        ${COMMON_LINK_LIBRARIES}
		        ${NCBI_VDB_LIBDIR}/libkxfs.${STLX}
		        ${NCBI_VDB_LIBDIR}/libkxml.${STLX}
		        ${COMMON_LIBS_READ}
		        xml2
		    )
	else()
		target_link_libraries( ccextract
		        ${COMMON_LINK_LIBRARIES}
		        kxfs
		        kxml
		        ${COMMON_LIBS_READ}
		        xml2
		    )
	endif()
	MakeLinksExe( ccextract false )
endif()
