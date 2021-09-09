add_compile_definitions( __mod__="tools/vdb-config" )

# Internal
add_executable( vdb-config
	configure
	util
	vdb-config
	interactive
	interactive2
	vdb-config-model
)
target_link_libraries ( vdb-config tui_cpp tui ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( vdb-config )