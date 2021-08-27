set( TARGET_NAME ngs-adapt-c++ )

set( SRC
	ReadCollectionItf
	PileupItf
	PileupEventItf
	ReferenceItf
	ReferenceSequenceItf
	AlignmentItf
	ReadGroupItf
	ReadItf
	FragmentItf
	StatisticsItf
	StringItf
	Refcount
	ErrorMsg
	ErrBlock
)

add_library( ${TARGET_NAME} ${SRC} )