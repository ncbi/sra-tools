add_compile_definitions( __mod__="tools/qual-recalib-stat" )

# Internal
add_executable( qual-recalib-stat
	namelist_tools
	context
	columns
	ref_exclude
	spot_position
	stat_mod_2
	reader
	writer
	qual-recalib-stat
)
target_link_libraries( qual-recalib-stat ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
