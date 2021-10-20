add_compile_definitions( __mod__="tools/ngs-pileup" )

include_directories( ${CMAKE_SOURCE_DIR} )

# Internal
add_executable( ngs-pileup ngs-pileup main )

set( NGS_PILEUP_LIBS
	ncbi-ngs-c++
	ngs-c++
	ngs
	ncbi-vdb
)

target_link_libraries( ngs-pileup ${NGS_PILEUP_LIBS} ${COMMON_LINK_LIBRARIES} )
