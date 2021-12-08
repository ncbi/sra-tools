add_compile_definitions( __mod__="tools/fasterq-dump" )

set( TOOLS_SRC
	helper
	arg_helper
	dflt_defline
	tool_ctx
	inspector
	sbuffer
	err_msg
	file_tools
	var_fmt
	flex_printer
	temp_dir
	progress_thread
	cleanup_task
	index
	lookup_writer
	lookup_reader
	locked_file_list
	locked_value
	file_printer
	merge_sorter
	sorter
	cmn_iter
	raw_read_iter
	special_iter
	fastq_iter
	join
	tbl_join
	temp_registry
	copy_machine
	concatenator
	fasterq-dump
)

add_executable( fasterq-dump "${TOOLS_SRC}" )
target_link_libraries( fasterq-dump ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( fasterq-dump true )
