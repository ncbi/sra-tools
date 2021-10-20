add_compile_definitions( __mod__="tools/ccextract" )

# External
if( LibXml2_FOUND )
	add_executable( ccextract ccextract )
	target_link_libraries( ccextract ${COMMON_LINK_LIBRARIES} kxfs kxml ncbi-vdb xml2 )
endif()
