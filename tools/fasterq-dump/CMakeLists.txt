add_compile_definitions( __mod__="tools/fasterq-dump" )

set( TOOLS_SRC
	helper
	temp_dir
	progress_thread
	cleanup_task
	index
	lookup_writer
	lookup_reader
	file_printer
	merge_sorter
	sorter
	cmn_iter
	raw_read_iter
	special_iter
	fastq_iter
	join
	tbl_join
	join_results
	temp_registry
	copy_machine
	concatenator
	fasterq-dump
)

if( WIN32 )
	set( ADDITIONAL_LIBS )
else()
	set( ADDITIONAL_LIBS )#m )
endif()

add_executable( fasterq-dump "${TOOLS_SRC}" )
target_link_libraries( fasterq-dump ${COMMON_LINK_LIBRARIES} ncbi-vdb ${ADDITIONAL_LIBS} )
