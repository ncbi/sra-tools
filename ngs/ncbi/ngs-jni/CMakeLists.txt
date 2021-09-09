add_compile_definitions (__mod__="ngs/ncbi/ngs-jni")

set( SRC
	jni_Manager
	jni_String
	jni_ErrorMsg
)

include_directories( ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-jni )
include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )

GenerateStaticLibs( ngs-jni "${SRC}" )

# TODO: java stuff, headers