add_compile_definitions( __mod__="tools/cg-load" )

# External

set( CGLOAD_SRC
	cg-load
	factory-evidence-intervals
	factory-evidence-dnbs
	factory-mappings
	factory-reads
	factory-tag-lfr
	f1_3
	f1_5
	f1_7
	f2_0
	f2_2
	f2_4
	f2_5
	file
	file-version-factory
	formats
	writer-algn
	writer-evidence-dnbs
	writer-evidence-intervals
	writer-seq
)

add_executable( cg-load ${CGLOAD_SRC} )

set( CGLOAD_LIB
	load
	${COMMON_LINK_LIBRARIES}
	${COMMON_LIBS_WRITE}
)
if( NOT WIN32 )
	list( APPEND CGLOAD_LIB m )
endif()

target_link_libraries( cg-load ${CGLOAD_LIB} )
MakeLinksExe( cg-load false )
