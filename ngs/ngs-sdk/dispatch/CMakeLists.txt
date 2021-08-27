set( TARGET_NAME ngs-disp )

set( SRC
	ReadCollectionItf
	ReferenceItf
	ReferenceSequenceItf
	PileupItf
	PileupEventItf
	AlignmentItf
	ReadGroupItf
	ReadItf
	FragmentItf
	StatisticsItf
	PackageItf
	StringItf
	Refcount
	VTable
	ErrBlock
	ErrorMsg
)

add_library( ${TARGET_NAME} ${SRC} )
