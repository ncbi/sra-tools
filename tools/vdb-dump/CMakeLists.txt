add_compile_definitions( __mod__="tools/vdb-dump" )

if( NOT WIN32 )
	set( ADDITIONAL_LIBS "pthread;dl;m" )
else()
	set( ADDITIONAL_LIBS "" )
endif()

add_executable( vdb-dump
	vdb-dump-context
	vdb-dump-coldefs
	vdb-dump-tools
	vdb-dump-str
	vdb-dump-helper
	vdb-dump-formats
	vdb-dump-redir
	vdb-dump-fastq
	vdb_info
	vdb-dump
)
target_link_libraries( vdb-dump ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} ${ADDITIONAL_LIBS} )
