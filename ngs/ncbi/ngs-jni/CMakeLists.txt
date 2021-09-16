add_compile_definitions (__mod__="ngs/ncbi/ngs-jni")

set( SRC
	jni_Manager
	jni_String
	jni_ErrorMsg
)

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-jni )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )
include_directories( ${CMAKE_SOURCE_DIR} )

GenerateStaticLibs( ngs-jni "${SRC}" )
