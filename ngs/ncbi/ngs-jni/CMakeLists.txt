add_compile_definitions (__mod__="ngs/ncbi/ngs-jni")

set( SRC
	jni_Manager
	jni_String
	jni_ErrorMsg
)


add_library( libngs-jni ${SRC} )
target_include_directories( libngs-jni PUBLIC ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
target_include_directories( libngs-jni PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-jni )
target_include_directories( libngs-jni PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )

# TODO: java stuff, headers