add_compile_definitions (__mod__="ngs/ncbi/ngs-py")

set( SRC
	py_Manager
)

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-py )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )

GenerateStaticLibs( ngs-py "${SRC}" )
