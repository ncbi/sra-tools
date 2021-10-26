set( SRC
	py_AlignmentItf
	py_AlignmentIteratorItf
	py_FragmentItf
	py_FragmentIteratorItf
	py_PackageItf
	py_PileupEventItf
	py_PileupEventIteratorItf
	py_PileupItf
	py_PileupIteratorItf
	py_ReadCollectionItf
	py_ReadGroupItf
	py_ReadGroupIteratorItf
	py_ReadItf
	py_ReadIteratorItf
	py_ReferenceItf
	py_ReferenceIteratorItf
	py_ReferenceSequenceItf
	py_StatisticsItf
	py_Refcount
	py_StringItf
)

include_directories( ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs-py )

GenerateStaticLibs( ngs-bind-python "${SRC}" )
