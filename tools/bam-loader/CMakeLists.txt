add_compile_definitions( __mod__="tools/bam-loader" )

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/ext/ ) # zlib.h

# Internal
add_executable( samview
	bam
	sam
	samview
)
target_link_libraries( samview ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
MakeLinksExe( samview true )

# External
if( NOT WIN32)
	add_executable( bam-load
		bam-loader
		bam
		sam
		alignment-writer
		reference-writer
		sequence-writer
		loader-imp
		mem-bank
		low-match-count
	)
	target_link_libraries( bam-load ${COMMON_LINK_LIBRARIES} load ncbi-wvdb.${STLX} pthread dl )
	MakeLinksExe( bam-load true )
endif()
