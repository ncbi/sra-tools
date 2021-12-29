add_compile_definitions (__mod__="ngs/ncbi/ngs-c++")

set( NGS_SRC
	NCBI-NGS
)

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )
include_directories( ${CMAKE_SOURCE_DIR} )

GenerateStaticLibs( ncbi-ngs-c++ "${NGS_SRC}" )
target_link_libraries( ncbi-ngs-c++ ngs )
ExportStatic( ncbi-ngs-c++ true )

if( NOT WIN32 )
	add_executable( testy-the-bear testy-the-bear )
	target_link_libraries( testy-the-bear ${COMMON_LINK_LIBRARIES} ncbi-ngs-c++ ncbi-ngs  )
endif()
