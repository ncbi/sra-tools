
set( SRC
	py_Manager
)

GenerateStaticLibs( ngs-py "${SRC}" )

target_include_directories( ngs-py PUBLIC ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
target_include_directories( ngs-py PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-py )
target_include_directories( ngs-py PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )

target_compile_definitions ( ngs-py PUBLIC __mod__="ngs/ncbi/ngs-py" )
