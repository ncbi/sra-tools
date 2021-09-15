if( WIN32 )
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
endif()

add_compile_definitions( __mod__="tools/sra-sort" )

# Internal
if( NOT WIN32 )
	add_executable( dump-blob-boundaries dump-blob-boundaries )
	target_link_libraries( dump-blob-boundaries ${COMMON_LINK_LIBRARIES} ncbi-vdb )
	MSVS_StaticRuntime( dump-blob-boundaries )
endif()

# External

set( SRA_SORT_SRC
	caps
	mem
	membank
	paged-membank
	paged-mmapbank
	except
	idx-mapping
	map-file
	col-pair
	row-set
	simple-row-set
	mapping-row-set
	sorting-row-set
	meta-pair
	dir-pair
	tbl-pair
	db-pair
	glob-poslen
	poslen-col-pair
	ref-alignid-col
	buff-writer
	id-mapper-col
	capture-first-half-aligned
	run
	sra-sort
	xcheck-ref-align

	csra-tbl
	csra-pair
)

add_executable( sra-sort ${SRA_SORT_SRC} )
target_link_libraries( sra-sort ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( sra-sort )

add_executable( sra-sort-cg ${SRA_SORT_SRC} )
target_link_libraries( sra-sort-cg ${COMMON_LINK_LIBRARIES} ncbi-wvdb )
MSVS_StaticRuntime( sra-sort-cg )
target_compile_definitions( sra-sort-cg PUBLIC SEQUENCE_BEFORE_SECONDARY=0 )