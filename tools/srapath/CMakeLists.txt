add_compile_definitions( __mod__="tools/srapath" )

# External
add_executable( srapath
	helper
	line_iter
	repository-mgr
	services
	cgi_request
	srapath
)
target_link_libraries( srapath ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( srapath )
