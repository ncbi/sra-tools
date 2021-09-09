add_compile_definitions( __mod__="tools/vcf-loader" )

add_executable( vcf-loader
	configure
	util
	vdb-config
	interactive
	interactive2
	vdb-config-model
)
target_link_libraries ( vcf-loader tui_cpp tui ${COMMON_LINK_LIBRARIES} ncbi-vdb )
MSVS_StaticRuntime( vcf-loader )
