add_compile_definitions (__mod__="ngs/ncbi/ngs-py")

set( SRC
	py_Manager
)


add_library( libngs-py ${SRC} )
target_include_directories( libngs-py PUBLIC ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
target_include_directories( libngs-py PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-py )
target_include_directories( libngs-py PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )
