add_compile_definitions( __mod__="tools/bam-loader" )

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/interfaces/ext/ ) # zlib.h

# Internal
add_executable( samview
	bam
	sam
	samview
)
if( WIN32 )
	target_link_libraries( samview ${COMMON_LINK_LIBRARIES} ncbi-vdb )
	MSVS_StaticRuntime( samview )
else()
	target_link_libraries( samview ${COMMON_LINK_LIBRARIES} ncbi-vdb m )
endif()

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
	target_link_libraries( bam-load ${COMMON_LINK_LIBRARIES} load ncbi-wvdb )
	MSVS_StaticRuntime( bam-load )
endif()
