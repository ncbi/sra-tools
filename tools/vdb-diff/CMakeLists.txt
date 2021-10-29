add_compile_definitions( __mod__="tools/vdb-diff" )

# External
add_executable( vdb-diff
	namelist_tools
	coldefs
	vdb-diff-context
	cmn
	row_by_row
	col_by_col
	vdb-diff
)
target_link_libraries( vdb-diff ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( vdb-diff true )
