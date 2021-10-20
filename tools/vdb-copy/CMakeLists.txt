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

add_custom_command( TARGET vdb-copy POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/vdb-copy.kfg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ncbi/vdb-copy.kfg
    COMMAND_EXPAND_LISTS
)