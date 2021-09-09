add_compile_definitions( __mod__="tools/vdb-copy" )

add_executable( vdb-copy
	context
	helper
	coldefs
	get_platform
	namelist_tools
	copy_meta
	type_matcher
	redactval
	config_values
	vdb-copy
)
target_link_libraries( vdb-copy ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( vdb-copy )