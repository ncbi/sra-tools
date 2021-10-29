add_compile_definitions( __mod__="tools/vdb-sql" )

# External
add_executable( vdb-sql vdb_shell )

set( VDB_SQL_LIB
	vdb-sqlite
	ngs
	${COMMON_LIBS_READ}
)
if( WIN32 )
	list( APPEND VDB_SQL_LIB
        libcmt$<$<CONFIG:Debug>:d>
        libcpmt$<$<CONFIG:Debug>:d>
        libucrt$<$<CONFIG:Debug>:d>
        libvcruntime$<$<CONFIG:Debug>:d>
    )
endif()

target_link_libraries( vdb-sql ${VDB_SQL_LIB} ${COMMON_LINK_LIBRARIES} )
MakeLinksExe( vdb-sql true )

add_executable( fastconv fastconv )
target_link_libraries( fastconv ${COMMON_LINK_LIBRARIES} ${VDB_SQL_LIB} )
MakeLinksExe( fastconv true )
