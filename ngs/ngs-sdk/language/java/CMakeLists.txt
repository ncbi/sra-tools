#add_compile_definitions( _ARCH_BITS=${BITS} )

set( TARGET_NAME ngs-bind-java )

set( SRC
	jni_ReadCollectionItf
	jni_ReferenceIteratorItf
	jni_ReferenceItf
	jni_ReferenceSequenceItf
	jni_PileupIteratorItf
	jni_PileupItf
	jni_PileupEventIteratorItf
	jni_PileupEventItf
	jni_AlignmentIteratorItf
	jni_AlignmentItf
	jni_ReadGroupIteratorItf
	jni_ReadGroupItf
	jni_ReadIteratorItf
	jni_ReadItf
	jni_FragmentItf
	jni_StatisticsItf
	jni_Package
	jni_String
	jni_Refcount
	jni_ErrorMsg
)

add_library( ${TARGET_NAME} ${SRC} )
target_include_directories( ${TARGET_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-jni )
