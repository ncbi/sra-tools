add_compile_definitions( __mod__="tools/vdb-sql" )

# External
add_executable( vdb-sql vdb_shell )

set( VDB_SQL_LIB
	vdb-sqlite
	ngs
	ncbi-vdb
	m
	pthread
	dl
)

target_link_libraries( vdb-sql ${VDB_SQL_LIB} )
MSVS_StaticRuntime( vdb-sql )


add_executable( fastconv fastconv )

target_link_libraries( fastconv ${COMMON_LINK_LIBRARIES} ${VDB_SQL_LIB} )
MSVS_StaticRuntime( fastconv )
